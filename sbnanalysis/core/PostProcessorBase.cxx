#include <algorithm>
#include <TBranch.h>
#include <TFile.h>
#include <TTree.h>
#include <TParameter.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>
#include <ROOT/TTreeProcessorMT.hxx>
#include "fhiclcpp/ParameterSet.h"
#include "Event.hh"
#include "Loader.hh"
#include "PostProcessorBase.hh"
#include "Experiment.hh"

#include "larcorealg/Geometry/GeometryCore.h"
#include "larcorealg/Geometry/AuxDetGeometryCore.h"
#include "larsim/MCCheater/BackTrackerService.h"
#include "larsim/MCCheater/PhotonBackTracker.h"
#include "lardataobj/Simulation/GeneratedParticleInfo.h"
#include "lardataalg/DetectorInfo/LArPropertiesStandard.h"
#include "lardataalg/DetectorInfo/DetectorPropertiesStandard.h"
#include "lardataalg/DetectorInfo/DetectorClocksStandard.h"

namespace core {

PostProcessorBase::PostProcessorBase(): fEvent(NULL), fProviderManager(NULL), fExperimentID(NULL), fConfigExperimentID(-1) {}


PostProcessorBase::~PostProcessorBase() {}


void PostProcessorBase::Initialize(char* config, const std::string &output_fname, unsigned n_threads) {
  fhicl::ParameterSet* cfg = LoadConfig(config);

  // setup config
  cfg->put("NThreads", n_threads);
  if (output_fname.size() != 0) cfg->put("OutputFile", output_fname);

  fConfigExperimentID = cfg->get("ExperimentID", -1);

  if (fConfigExperimentID >= 0) {
    fProviderManager = new ProviderManager((Experiment)fConfigExperimentID, "", false); 
  }

  Initialize(cfg);
  fSubRun = 0;
  fFileMeta = 0;
  fEvent = 0;
  fExperimentID = 0;
}


void PostProcessorBase::Run(std::vector<std::string> inputFiles) {
  for (auto const& fname: inputFiles) {
    // get ROOT file
    TFile f(fname.c_str());
    if (f.IsZombie()) {
      std::cerr << "Failed openning file: " << fname << ". "
                << "Cleaning up and exiting." << std::endl;
      break;
    }


    // first file -- setup the provider manager
    if (fExperimentID == NULL) {
      // get the Experiment ID
      f.GetObject("experiment", fExperimentID);
      if (fConfigExperimentID >= 0) assert(fConfigExperimentID == fExperimentID->GetVal());
      else if ((Experiment)fExperimentID->GetVal() != kExpOther) {
        fProviderManager = new ProviderManager((Experiment)fExperimentID->GetVal(), "", false); 
      }
    }
    // otherwise -- check if we can re-use
    else {
      Experiment last_experiment_id = (Experiment)fExperimentID->GetVal();
      // get the Experiment ID
      f.GetObject("experiment", fExperimentID);
      if (fConfigExperimentID >= 0) assert(fConfigExperimentID == fExperimentID->GetVal());
      else if ((Experiment)fExperimentID->GetVal() != kExpOther) {
        if ((Experiment)fExperimentID->GetVal() == last_experiment_id) {} // reuse provider manager 
        else {
          delete fProviderManager;
          fProviderManager = new ProviderManager((Experiment)fExperimentID->GetVal(), "", false);
        }
      }
      else {
        if (fProviderManager != NULL) {
          delete fProviderManager;
          fProviderManager = NULL;
        }
      }
    }

    f.GetObject("sbnana", fEventTree);
    ROOT::TTreeProcessorMT tp(*fEventTree);
    FileSetup(&f);

    std::atomic_uint16_t index = 0;
    tp.Process([&](TTreeReader &tree) {
      TTreeReaderValue<event::Event> event(tree, "events");
      uint16_t this_index = index.fetch_add(1);
      std::cerr << "Index: " << this_index << std::endl;
      EventTreeSetup(tree, this_index);
      while (tree.Next()) {
        ProcessEvent(event.Get(), this_index);
      }
    });

    // process all subruns
    f.GetObject("sbnsubrun", fSubRunTree);
    if (fSubRunTree == NULL) {
      std::cerr << "Error: NULL subrun tree" << std::endl;
    }
    fSubRunTree->SetBranchAddress("subruns", &fSubRun);

    for (int subrun_ind = 0; subrun_ind < fSubRunTree->GetEntries(); subrun_ind++) {
      fSubRunTree->GetEntry(subrun_ind);
      ProcessSubRun(fSubRun);
    }

    // process all the file meta-data
    f.GetObject("sbnfilemeta", fFileMetaTree);
    if (fFileMetaTree == NULL) {
      std::cerr << "Error: NULL filemeta tree" << std::endl;
    }
    fFileMetaTree->SetBranchAddress("filemeta", &fFileMeta);
    for (int filemeta_ind = 0; filemeta_ind < fFileMetaTree->GetEntries(); filemeta_ind++) {
      fFileMetaTree->GetEntry(filemeta_ind);
      ProcessFileMeta(fFileMeta);
    } 

    FileCleanup(fEventTree);
  } 

  if (fProviderManager != NULL) {
    delete fProviderManager;
    fProviderManager = NULL;
  }

  // teardown
  Finalize();
}

}  // namespace core

