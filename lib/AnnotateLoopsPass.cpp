//
//
//

#include "AnnotateLoopsPass.hpp"

#include "AnnotateLoops.hpp"

#include "BWList.hpp"

#include "llvm/IR/Module.h"
// using llvm::Module

#include "llvm/Analysis/LoopInfo.h"
// using llvm::LoopInfoWrapperPass
// using llvm::LoopInfo
// using llvm::Loop

#include "llvm/IR/LegacyPassManager.h"
// using llvm::PassManagerBase

#include "llvm/Transforms/IPO/PassManagerBuilder.h"
// using llvm::PassManagerBuilder
// using llvm::RegisterStandardPasses

#include "llvm/ADT/SmallVector.h"
// using llvm::SmallVector

#include "llvm/Support/raw_ostream.h"
// using llvm::raw_ostream

#include "llvm/Support/FileSystem.h"
// using llvm::sys::fs::OpenFlags

#include "llvm/Support/CommandLine.h"
// using llvm::cl::opt
// using llvm::cl::desc

#include "llvm/Support/Debug.h"
// using DEBUG macro
// using llvm::dbgs

#include <algorithm>
// using std::for_each

#include <string>
// using std::string

#include <utility>
// using std::pair

#include <map>
// using std::map

#include <fstream>
// using std::ifstream

#include <system_error>
// using std::error_code

#include <cstring>
// using std::strncmp

#define DEBUG_TYPE "annotate_loops"

#define STRINGIFY_UTIL(x) #x
#define STRINGIFY(x) STRINGIFY_UTIL(x)

#define PRJ_CMDLINE_DESC(x) x " (version: " STRINGIFY(VERSION_STRING) ")"

#ifndef NDEBUG
#define PLUGIN_OUT llvm::outs()
//#define PLUGIN_OUT llvm::nulls()

// convenience macro when building against a NDEBUG LLVM
#undef DEBUG
#define DEBUG(X)                                                               \
  do {                                                                         \
    X;                                                                         \
  } while (0);
#else // NDEBUG
#define PLUGIN_OUT llvm::dbgs()
#endif // NDEBUG

#define PLUGIN_ERR llvm::errs()

// plugin registration for opt

char icsa::AnnotateLoopsPass::ID = 0;
static llvm::RegisterPass<icsa::AnnotateLoopsPass>
    X("annotate-loops", PRJ_CMDLINE_DESC("annotate loops"), false, false);

// plugin registration for clang

// the solution was at the bottom of the header file
// 'llvm/Transforms/IPO/PassManagerBuilder.h'
// create a static free-floating callback that uses the legacy pass manager to
// add an instance of this pass and a static instance of the
// RegisterStandardPasses class

static void registerAnnotateLoopsPass(const llvm::PassManagerBuilder &Builder,
                                      llvm::legacy::PassManagerBase &PM) {
  PM.add(new icsa::AnnotateLoopsPass());

  return;
}

static llvm::RegisterStandardPasses
    RegisterAnnotateLoopsPass(llvm::PassManagerBuilder::EP_EarlyAsPossible,
                              registerAnnotateLoopsPass);

//

enum struct ALOpts {
  write,
  read,
};

static llvm::cl::opt<ALOpts> OperationMode(
    "al-mode", llvm::cl::desc("operation mode"), llvm::cl::init(ALOpts::write),
    llvm::cl::values(clEnumValN(ALOpts::write, "write",
                                "write looops with annotated id mode"),
                     clEnumValN(ALOpts::read, "read",
                                "read loops with annotated id mode"),
                     nullptr));

static llvm::cl::opt<unsigned int>
    LoopDepthThreshold("al-loop-depth-threshold",
                       llvm::cl::desc("loop depth threshold"),
                       llvm::cl::init(1));

static llvm::cl::opt<unsigned int> LoopStartId("al-loop-start-id",
                                               llvm::cl::desc("loop start id"),
                                               llvm::cl::init(1));

static llvm::cl::opt<unsigned int>
    LoopIdInterval("al-loop-id-interval", llvm::cl::desc("loop id interval"),
                   llvm::cl::init(1));
//

static llvm::cl::opt<std::string>
    ReportStatsFilename("al-stats",
                        llvm::cl::desc("annotate loops stats report filename"));

static llvm::cl::opt<std::string>
    FuncWhiteListFilename("al-fn-whitelist",
                          llvm::cl::desc("function whitelist"));

namespace icsa {

namespace {

using FunctionName_t = std::string;
using LoopIdRange_t =
    std::pair<AnnotateLoops::LoopID_t, AnnotateLoops::LoopID_t>;

long NumFunctionsProcessed = 0;
std::map<FunctionName_t, LoopIdRange_t> FunctionsAltered;

std::map<AnnotateLoops::LoopID_t, FunctionName_t> LoopsAnnotated;

void ReportStats(const char *Filename) {
  std::error_code err;
  llvm::raw_fd_ostream report(Filename, err, llvm::sys::fs::F_Text);

  if (err)
    PLUGIN_ERR << "could not open file: \"" << Filename
               << "\" reason: " << err.message() << "\n";
  else {
    report << NumFunctionsProcessed << "\n";

    for (const auto &e : FunctionsAltered)
      report << e.first << " " << e.second.first << " " << e.second.second
             << "\n";

    if (FunctionsAltered.size())
      report << "--\n";

    for (const auto &e : LoopsAnnotated)
      report << e.first << " " << e.second << "\n";
  }

  return;
}

} // namespace anonymous end

void AnnotateLoopsPass::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.addRequired<llvm::LoopInfoWrapperPass>();
  AU.setPreservesAll();

  return;
}

bool AnnotateLoopsPass::runOnModule(llvm::Module &CurModule) {
  bool shouldReportStats = !ReportStatsFilename.empty();
  bool useFuncWhitelist = !FuncWhiteListFilename.empty();
  bool hasChanged = false;

  BWList funcWhiteList;
  if (useFuncWhitelist) {
    std::ifstream funcWhiteListFile{FuncWhiteListFilename};

    if (funcWhiteListFile.is_open()) {
      funcWhiteList.addRegex(funcWhiteListFile);
      funcWhiteListFile.close();
    } else
      PLUGIN_ERR << "could not open file: \'" << FuncWhiteListFilename
                 << "\'\n";
  }

  llvm::SmallVector<llvm::Loop *, 16> workList;
  AnnotateLoops annotator{LoopStartId, LoopIdInterval};

  for (auto &CurFunc : CurModule) {
    if (useFuncWhitelist && !funcWhiteList.matches(CurFunc.getName().data()))
      continue;

    if (CurFunc.isDeclaration())
      continue;

    NumFunctionsProcessed++;
    workList.clear();
    auto &LI = getAnalysis<llvm::LoopInfoWrapperPass>(CurFunc).getLoopInfo();

    std::for_each(LI.begin(), LI.end(),
                  [&workList](llvm::Loop *e) { workList.push_back(e); });

    for (auto i = 0; i < workList.size(); ++i)
      for (auto &e : workList[i]->getSubLoops())
        workList.push_back(e);

    workList.erase(
        std::remove_if(workList.begin(), workList.end(), [](const auto *e) {
          auto d = e->getLoopDepth();
          return d > LoopDepthThreshold;
        }), workList.end());

    auto rangeStart = annotator.getId();

    if (ALOpts::write == OperationMode)
      for (auto *e : workList) {
        auto id = annotator.getId();
        annotator.annotateWithId(*e);

        if (shouldReportStats)
          LoopsAnnotated.emplace(id, CurFunc.getName().str());
      }
    else
      for (auto *e : workList)
        if (annotator.hasAnnotatedId(*e)) {
          auto id = annotator.getAnnotatedId(*e);

          if (shouldReportStats)
            LoopsAnnotated.emplace(id, CurFunc.getName().str());
        }

    auto rangeOpenEnd = annotator.getId();

    if (shouldReportStats && ALOpts::write == OperationMode &&
        workList.size()) {
      FunctionsAltered.emplace(CurFunc.getName(),
                               std::make_pair(rangeStart, rangeOpenEnd));
      hasChanged = true;
    }
  }

  if (shouldReportStats)
    ReportStats(ReportStatsFilename.c_str());

  return hasChanged;
}

} // namespace icsa end
