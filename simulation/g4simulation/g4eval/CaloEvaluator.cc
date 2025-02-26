#include "CaloEvaluator.h"

#include "CaloEvalStack.h"
#include "CaloRawClusterEval.h"
#include "CaloRawTowerEval.h"
#include "CaloTruthEval.h"

#include <g4main/PHG4Particle.h>
#include <g4main/PHG4Shower.h>
#include <g4main/PHG4TruthInfoContainer.h>
#include <g4main/PHG4VtxPoint.h>

#include <globalvertex/GlobalVertex.h>
#include <globalvertex/GlobalVertexMap.h>

#include <calobase/RawCluster.h>
#include <calobase/RawClusterContainer.h>
#include <calobase/RawClusterUtility.h>
#include <calobase/RawTower.h>
#include <calobase/RawTowerContainer.h>
#include <calobase/RawTowerGeom.h>
#include <calobase/RawTowerGeomContainer.h>
#include <calobase/TowerInfo.h>
#include <calobase/TowerInfoContainer.h>

#include <fun4all/Fun4AllReturnCodes.h>
#include <fun4all/SubsysReco.h>

#include <phool/getClass.h>
#include <phool/phool.h>

#include <TFile.h>
#include <TNtuple.h>
#include <TTree.h>

#include <CLHEP/Vector/ThreeVector.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <utility>

CaloEvaluator::CaloEvaluator(const std::string& name, const std::string& caloname, const std::string& filename)
  : SubsysReco(name)
  , _caloname(caloname)
  , _filename(filename)
{
}

int CaloEvaluator::Init(PHCompositeNode* /*topNode*/)
{
  delete _tfile;  // make cppcheck happy
  _tfile = new TFile(_filename.c_str(), "RECREATE");

  if (_do_gpoint_eval)
  {
    _ntp_gpoint = new TNtuple("ntp_gpoint", "primary vertex => best (first) vertex",
                              "event:gvx:gvy:gvz:"
                              "vx:vy:vz");
  }

  if (_do_gshower_eval)
  {
    _ntp_gshower = new TNtuple("ntp_gshower", "truth shower => best cluster",
                               "event:gparticleID:gflavor:gnhits:"
                               "geta:gphi:ge:gpt:gvx:gvy:gvz:gembed:gedep:"
                               "clusterID:ntowers:eta:x:y:z:phi:e:efromtruth");
  }

  // Barak: Added TTree to will allow the TowerID to be set correctly as integer
  if (_do_tower_eval)
  {
    _ntp_tower = new TNtuple("ntp_tower", "tower => max truth primary",
                             "event:towerID:ieta:iphi:eta:phi:e:x:y:z:"
                             "gparticleID:gflavor:gnhits:"
                             "geta:gphi:ge:gpt:gvx:gvy:gvz:"
                             "gembed:gedep:"
                             "efromtruth");

    // Make Tree
    _tower_debug = new TTree("tower_debug", "tower => max truth primary");

    _tower_debug->Branch("event", &_ievent, "event/I");
    _tower_debug->Branch("towerID", &_towerID_debug, "towerID/I");
    _tower_debug->Branch("ieta", &_ieta_debug, "ieta/I");
    _tower_debug->Branch("iphi", &_iphi_debug, "iphi/I");
    _tower_debug->Branch("eta", &_eta_debug, "eta/F");
    _tower_debug->Branch("phi", &_phi_debug, "phi/F");
    _tower_debug->Branch("e", &_e_debug, "e/F");
    _tower_debug->Branch("x", &_x_debug, "x/F");
    _tower_debug->Branch("y", &_y_debug, "y/F");
    _tower_debug->Branch("z", &_z_debug, "z/F");
  }

  if (_do_cluster_eval)
  {
    _ntp_cluster = new TNtuple("ntp_cluster", "cluster => max truth primary",
                               "event:clusterID:ntowers:eta:x:y:z:phi:e:"
                               "gparticleID:gflavor:gnhits:"
                               "geta:gphi:ge:gpt:gvx:gvy:gvz:"
                               "gembed:gedep:"
                               "efromtruth");
  }

  return Fun4AllReturnCodes::EVENT_OK;
}

int CaloEvaluator::process_event(PHCompositeNode* topNode)
{
  if (!_caloevalstack)
  {
    _caloevalstack = new CaloEvalStack(topNode, _caloname);
    _caloevalstack->set_strict(_strict);
    _caloevalstack->set_verbosity(Verbosity() + 1);
  }
  else
  {
    _caloevalstack->next_event(topNode);
  }

  //-----------------------------------
  // print what is coming into the code
  //-----------------------------------

  printInputInfo(topNode);

  //---------------------------
  // fill the Evaluator NTuples
  //---------------------------

  fillOutputNtuples(topNode);

  //--------------------------------------------------
  // Print out the ancestry information for this event
  //--------------------------------------------------

  printOutputInfo(topNode);

  ++_ievent;
  ++m_EvtCounter;

  return Fun4AllReturnCodes::EVENT_OK;
}

int CaloEvaluator::End(PHCompositeNode* /*topNode*/)
{
  _tfile->cd();

  if (_do_gpoint_eval)
  {
    _ntp_gpoint->Write();
  }
  if (_do_gshower_eval)
  {
    _ntp_gshower->Write();
  }
  if (_do_tower_eval)
  {
    _ntp_tower->Write();
    _tower_debug->Write();
  }
  if (_do_cluster_eval)
  {
    _ntp_cluster->Write();
  }

  _tfile->Close();

  delete _tfile;

  if (Verbosity() > 0)
  {
    std::cout << "========================= " << Name() << "::End() ============================" << std::endl;
    std::cout << " " << m_EvtCounter << " events of output written to: " << _filename << std::endl;
    std::cout << "===========================================================================" << std::endl;
  }

  if (_caloevalstack)
  {
    delete _caloevalstack;
  }

  return Fun4AllReturnCodes::EVENT_OK;
}

void CaloEvaluator::printInputInfo(PHCompositeNode* topNode)
{
  if (Verbosity() > 2)
  {
    std::cout << "CaloEvaluator::printInputInfo() entered" << std::endl;
  }

  // print out the truth container

  if (Verbosity() > 1)
  {
    std::cout << std::endl;
    std::cout << PHWHERE << "   NEW INPUT FOR EVENT " << _ievent << std::endl;
    std::cout << std::endl;

    // need things off of the DST...
    PHG4TruthInfoContainer* truthinfo = findNode::getClass<PHG4TruthInfoContainer>(topNode, "G4TruthInfo");
    if (!truthinfo)
    {
      std::cout << PHWHERE << " ERROR: Can't find G4TruthInfo" << std::endl;
      exit(-1);
    }

    std::cout << Name() << ": PHG4TruthInfoContainer contents: " << std::endl;

    PHG4TruthInfoContainer::Range truthrange = truthinfo->GetParticleRange();
    for (PHG4TruthInfoContainer::Iterator truthiter = truthrange.first;
         truthiter != truthrange.second;
         ++truthiter)
    {
      PHG4Particle* particle = truthiter->second;

      std::cout << truthiter->first << " => pid: " << particle->get_pid()
                << " pt: " << sqrt(pow(particle->get_px(), 2) + pow(particle->get_py(), 2)) << std::endl;
    }
  }

  return;
}

void CaloEvaluator::printOutputInfo(PHCompositeNode* topNode)
{
  if (Verbosity() > 2)
  {
    std::cout << "CaloEvaluator::printOutputInfo() entered" << std::endl;
  }

  CaloRawClusterEval* clustereval = _caloevalstack->get_rawcluster_eval();
  clustereval->set_usetowerinfo(_use_towerinfo);
  CaloTruthEval* trutheval = _caloevalstack->get_truth_eval();

  //==========================================
  // print out some useful stuff for debugging
  //==========================================

  if (Verbosity() > 1)
  {
    // event information
    std::cout << std::endl;
    std::cout << PHWHERE << "   NEW OUTPUT FOR EVENT " << _ievent << std::endl;
    std::cout << std::endl;

    // need things off of the DST...
    PHG4TruthInfoContainer* truthinfo = findNode::getClass<PHG4TruthInfoContainer>(topNode, "G4TruthInfo");
    if (!truthinfo)
    {
      std::cout << PHWHERE << " ERROR: Can't find G4TruthInfo" << std::endl;
      exit(-1);
    }

    // need things off of the DST...
    GlobalVertexMap* vertexmap = findNode::getClass<GlobalVertexMap>(topNode, "GlobalVertexMap");

    PHG4VtxPoint* gvertex = truthinfo->GetPrimaryVtx(truthinfo->GetPrimaryVertexIndex());
    float gvx = gvertex->get_x();
    float gvy = gvertex->get_y();
    float gvz = gvertex->get_z();

    float vx = NAN;
    float vy = NAN;
    float vz = NAN;
    if (vertexmap)
    {
      if (!vertexmap->empty())
      {
        GlobalVertex* vertex = (vertexmap->begin()->second);

        vx = vertex->get_x();
        vy = vertex->get_y();
        vz = vertex->get_z();
      }
    }

    std::cout << "vtrue = (" << gvx << "," << gvy << "," << gvz << ") => vreco = (" << vx << "," << vy << "," << vz << ")" << std::endl;

    PHG4TruthInfoContainer::ConstRange range = truthinfo->GetPrimaryParticleRange();
    for (PHG4TruthInfoContainer::ConstIterator iter = range.first;
         iter != range.second;
         ++iter)
    {
      PHG4Particle* primary = iter->second;

      std::cout << std::endl;

      std::cout << "===Primary PHG4Particle=========================================" << std::endl;
      std::cout << " particle id = " << primary->get_track_id() << std::endl;
      std::cout << " flavor = " << primary->get_pid() << std::endl;
      std::cout << " (px,py,pz,e) = (";

      float gpx = primary->get_px();
      float gpy = primary->get_py();
      float gpz = primary->get_pz();
      float ge = primary->get_e();

      std::cout.width(5);
      std::cout << gpx;
      std::cout << ",";
      std::cout.width(5);
      std::cout << gpy;
      std::cout << ",";
      std::cout.width(5);
      std::cout << gpz;
      std::cout << ",";
      std::cout.width(5);
      std::cout << ge;
      std::cout << ")" << std::endl;

      float gpt = std::sqrt(gpx * gpx + gpy * gpy);
      float geta = NAN;
      if (gpt != 0.0)
      {
        geta = std::asinh(gpz / gpt);
      }
      float gphi = std::atan2(gpy, gpx);

      std::cout << "(eta,phi,e,pt) = (";
      std::cout.width(5);
      std::cout << geta;
      std::cout << ",";
      std::cout.width(5);
      std::cout << gphi;
      std::cout << ",";
      std::cout.width(5);
      std::cout << ge;
      std::cout << ",";
      std::cout.width(5);
      std::cout << gpt;
      std::cout << ")" << std::endl;

      PHG4VtxPoint* vtx = trutheval->get_vertex(primary);
      float local_gvx = vtx->get_x();
      float local_gvy = vtx->get_y();
      float local_gvz = vtx->get_z();

      std::cout << " vtrue = (";
      std::cout.width(5);
      std::cout << local_gvx;
      std::cout << ",";
      std::cout.width(5);
      std::cout << local_gvy;
      std::cout << ",";
      std::cout.width(5);
      std::cout << local_gvz;
      std::cout << ")" << std::endl;

      std::cout << " embed = " << trutheval->get_embed(primary) << std::endl;
      std::cout << " edep = " << trutheval->get_shower_energy_deposit(primary) << std::endl;

      std::set<RawCluster*> clusters = clustereval->all_clusters_from(primary);
      for (auto cluster : clusters)
      {
        float ntowers = cluster->getNTowers();
        float x = cluster->get_x();
        float y = cluster->get_y();
        float z = cluster->get_z();
        float phi = cluster->get_phi();
        float e = cluster->get_energy();

        float efromtruth = clustereval->get_energy_contribution(cluster, primary);

        std::cout << " => #" << cluster->get_id() << " (x,y,z,phi,e) = (";
        std::cout.width(5);
        std::cout << x;
        std::cout << ",";
        std::cout.width(5);
        std::cout << y;
        std::cout << ",";
        std::cout.width(5);
        std::cout << z;
        std::cout << ",";
        std::cout.width(5);
        std::cout << phi;
        std::cout << ",";
        std::cout.width(5);
        std::cout << e;
        std::cout << "), ntowers = " << ntowers << ", efromtruth = " << efromtruth << std::endl;
      }
    }
    std::cout << std::endl;
  }

  return;
}

void CaloEvaluator::fillOutputNtuples(PHCompositeNode* topNode)
{
  if (Verbosity() > 2)
  {
    std::cout << "CaloEvaluator::fillOutputNtuples() entered" << std::endl;
  }

  CaloRawClusterEval* clustereval = _caloevalstack->get_rawcluster_eval();
  CaloRawTowerEval* towereval = _caloevalstack->get_rawtower_eval();
  CaloTruthEval* trutheval = _caloevalstack->get_truth_eval();

  //----------------------
  // fill the Event NTuple
  //----------------------

  if (_do_gpoint_eval)
  {
    // need things off of the DST...
    PHG4TruthInfoContainer* truthinfo = findNode::getClass<PHG4TruthInfoContainer>(topNode, "G4TruthInfo");
    if (!truthinfo)
    {
      std::cout << PHWHERE << " ERROR: Can't find G4TruthInfo" << std::endl;
      exit(-1);
    }

    // need things off of the DST...
    GlobalVertexMap* vertexmap = findNode::getClass<GlobalVertexMap>(topNode, "GlobalVertexMap");

    PHG4VtxPoint* gvertex = truthinfo->GetPrimaryVtx(truthinfo->GetPrimaryVertexIndex());
    float gvx = gvertex->get_x();
    float gvy = gvertex->get_y();
    float gvz = gvertex->get_z();

    float vx = NAN;
    float vy = NAN;
    float vz = NAN;
    if (vertexmap)
    {
      if (!vertexmap->empty())
      {
        GlobalVertex* vertex = (vertexmap->begin()->second);

        vx = vertex->get_x();
        vy = vertex->get_y();
        vz = vertex->get_z();
      }
    }

    float gpoint_data[7] = {(float) _ievent,
                            gvx,
                            gvy,
                            gvz,
                            vx,
                            vy,
                            vz};

    _ntp_gpoint->Fill(gpoint_data);
  }

  //------------------------
  // fill the Gshower NTuple
  //------------------------

  if (_ntp_gshower)
  {
    if (Verbosity() > 1)
    {
      std::cout << Name() << " CaloEvaluator::filling gshower ntuple..." << std::endl;
    }

    GlobalVertexMap* vertexmap = findNode::getClass<GlobalVertexMap>(topNode, "GlobalVertexMap");

    PHG4TruthInfoContainer* truthinfo = findNode::getClass<PHG4TruthInfoContainer>(topNode, "G4TruthInfo");
    if (!truthinfo)
    {
      std::cout << PHWHERE << " ERROR: Can't find G4TruthInfo" << std::endl;
      exit(-1);
    }

    PHG4TruthInfoContainer::ConstRange range = truthinfo->GetPrimaryParticleRange();
    for (PHG4TruthInfoContainer::ConstIterator iter = range.first;
         iter != range.second;
         ++iter)
    {
      PHG4Particle* primary = iter->second;

      if (primary->get_e() < _truth_e_threshold)
      {
        continue;
      }

      if (!_truth_trace_embed_flags.empty())
      {
        if (_truth_trace_embed_flags.find(trutheval->get_embed(primary)) ==
            _truth_trace_embed_flags.end())
        {
          continue;
        }
      }

      float gparticleID = primary->get_track_id();
      float gflavor = primary->get_pid();

      PHG4Shower* shower = trutheval->get_primary_shower(primary);
      float gnhits = NAN;
      if (shower)
      {
        gnhits = shower->get_nhits(trutheval->get_caloid());
      }
      else
      {
        gnhits = 0.0;
      }
      float gpx = primary->get_px();
      float gpy = primary->get_py();
      float gpz = primary->get_pz();
      float ge = primary->get_e();

      float gpt = std::sqrt(gpx * gpx + gpy * gpy);
      float geta = NAN;
      if (gpt != 0.0)
      {
        geta = std::asinh(gpz / gpt);
      }
      float gphi = std::atan2(gpy, gpx);

      PHG4VtxPoint* vtx = trutheval->get_vertex(primary);
      float gvx = vtx->get_x();
      float gvy = vtx->get_y();
      float gvz = vtx->get_z();

      float gembed = trutheval->get_embed(primary);
      float gedep = trutheval->get_shower_energy_deposit(primary);

      RawCluster* cluster = clustereval->best_cluster_from(primary);

      float clusterID = NAN;
      float ntowers = NAN;
      float eta = NAN;
      float x = NAN;
      float y = NAN;
      float z = NAN;
      float phi = NAN;
      float e = NAN;

      float efromtruth = NAN;

      if (cluster)
      {
        clusterID = cluster->get_id();
        ntowers = cluster->getNTowers();
        x = cluster->get_x();
        y = cluster->get_y();
        z = cluster->get_z();
        phi = cluster->get_phi();
        e = cluster->get_energy();

        efromtruth = clustereval->get_energy_contribution(cluster, primary);

        // require vertex for cluster eta calculation
        if (vertexmap)
        {
          if (!vertexmap->empty())
          {
            GlobalVertex* vertex = (vertexmap->begin()->second);

            eta =
                RawClusterUtility::GetPseudorapidity(
                    *cluster,
                    CLHEP::Hep3Vector(vertex->get_x(), vertex->get_y(), vertex->get_z()));
          }
        }
      }

      float shower_data[] = {(float) _ievent,
                             gparticleID,
                             gflavor,
                             gnhits,
                             geta,
                             gphi,
                             ge,
                             gpt,
                             gvx,
                             gvy,
                             gvz,
                             gembed,
                             gedep,
                             clusterID,
                             ntowers,
                             eta,
                             x,
                             y,
                             z,
                             phi,
                             e,
                             efromtruth};

      _ntp_gshower->Fill(shower_data);
    }
  }

  //----------------------
  // fill the Tower NTuple
  //----------------------

  if (_do_tower_eval)
  {
    if (!_use_towerinfo)
    {
      if (Verbosity() > 1)
      {
        std::cout << "CaloEvaluator::filling tower ntuple..." << std::endl;
      }

      std::string towernode = "TOWER_CALIB_" + _caloname;
      RawTowerContainer* towers = findNode::getClass<RawTowerContainer>(topNode, towernode.c_str());
      if (!towers)
      {
        std::cout << PHWHERE << " ERROR: Can't find " << towernode << std::endl;
        exit(-1);
      }

      std::string towergeomnode = "TOWERGEOM_" + _caloname;
      RawTowerGeomContainer* towergeom = findNode::getClass<RawTowerGeomContainer>(topNode, towergeomnode.c_str());
      if (!towergeom)
      {
        std::cout << PHWHERE << " ERROR: Can't find " << towergeomnode << std::endl;
        exit(-1);
      }

      RawTowerContainer::ConstRange begin_end = towers->getTowers();
      RawTowerContainer::ConstIterator rtiter;
      for (rtiter = begin_end.first; rtiter != begin_end.second; ++rtiter)
      {
        RawTower* tower = rtiter->second;

        if (tower->get_energy() < _reco_e_threshold)
        {
          continue;
        }

        RawTowerGeom* tower_geom = towergeom->get_tower_geometry(tower->get_id());
        if (!tower_geom)
        {
          std::cout << PHWHERE << " ERROR: Can't find tower geometry for this tower hit: ";
          tower->identify();
          exit(-1);
        }

        // std::cout<<"Tower ID = "<<tower->get_id()<<" for bin(j,k)= "<<tower->get_bineta()<<","<<tower->get_binphi()<<std::endl; //Added by Barak
        const float towerid = tower->get_id();
        const float ieta = tower->get_bineta();
        const float iphi = tower->get_binphi();
        const float eta = tower_geom->get_eta();
        const float phi = tower_geom->get_phi();
        const float e = tower->get_energy();
        const float x = tower_geom->get_center_x();
        const float y = tower_geom->get_center_y();
        const float z = tower_geom->get_center_z();

        // Added by Barak
        _towerID_debug = tower->get_id();
        _ieta_debug = tower->get_bineta();
        _iphi_debug = tower->get_binphi();
        _eta_debug = tower_geom->get_eta();
        _phi_debug = tower_geom->get_phi();
        _e_debug = tower->get_energy();
        _x_debug = tower_geom->get_center_x();
        _y_debug = tower_geom->get_center_y();
        _z_debug = tower_geom->get_center_z();

        PHG4Particle* primary = towereval->max_truth_primary_particle_by_energy(tower);

        float gparticleID = NAN;
        float gflavor = NAN;
        float gnhits = NAN;
        float gpx = NAN;
        float gpy = NAN;
        float gpz = NAN;
        float ge = NAN;

        float gpt = NAN;
        float geta = NAN;
        float gphi = NAN;

        float gvx = NAN;
        float gvy = NAN;
        float gvz = NAN;

        float gembed = NAN;
        float gedep = NAN;

        float efromtruth = NAN;

        if (primary)
        {
          gparticleID = primary->get_track_id();
          gflavor = primary->get_pid();

          PHG4Shower* shower = trutheval->get_primary_shower(primary);
          if (shower)
          {
            gnhits = shower->get_nhits(trutheval->get_caloid());
          }
          else
          {
            gnhits = 0.0;
          }
          gpx = primary->get_px();
          gpy = primary->get_py();
          gpz = primary->get_pz();
          ge = primary->get_e();

          gpt = std::sqrt(gpx * gpx + gpy * gpy);
          if (gpt != 0.0)
          {
            geta = std::asinh(gpz / gpt);
          }
          gphi = std::atan2(gpy, gpx);

          PHG4VtxPoint* vtx = trutheval->get_vertex(primary);

          if (vtx)
          {
            gvx = vtx->get_x();
            gvy = vtx->get_y();
            gvz = vtx->get_z();
          }

          gembed = trutheval->get_embed(primary);
          gedep = trutheval->get_shower_energy_deposit(primary);

          efromtruth = towereval->get_energy_contribution(tower, primary);
        }

        float tower_data[] = {(float) _ievent,
                              towerid,
                              ieta,
                              iphi,
                              eta,
                              phi,
                              e,
                              x,
                              y,
                              z,
                              gparticleID,
                              gflavor,
                              gnhits,
                              geta,
                              gphi,
                              ge,
                              gpt,
                              gvx,
                              gvy,
                              gvz,
                              gembed,
                              gedep,
                              efromtruth};

        _ntp_tower->Fill(tower_data);
        _tower_debug->Fill();  // Added by Barak (see above for explanation)
      }
    }
    else
    {
      if (Verbosity() > 1)
      {
        std::cout << "CaloEvaluator::filling tower ntuple..." << std::endl;
      }

      std::string towernode = "TOWERINFO_CALIB_" + _caloname;
      TowerInfoContainer* towers = findNode::getClass<TowerInfoContainer>(topNode, towernode.c_str());
      if (!towers)
      {
        std::cout << PHWHERE << " ERROR: Can't find " << towernode << std::endl;
        exit(-1);
      }

      std::string towergeomnode = "TOWERGEOM_" + _caloname;
      RawTowerGeomContainer* towergeom = findNode::getClass<RawTowerGeomContainer>(topNode, towergeomnode.c_str());
      if (!towergeom)
      {
        std::cout << PHWHERE << " ERROR: Can't find " << towergeomnode << std::endl;
        exit(-1);
      }

      unsigned int ntowers = towers->size();

      for (unsigned int channel = 0; channel < ntowers; channel++)
      {
        TowerInfo* tower = towers->get_tower_at_channel(channel);

        if (tower->get_energy() < _reco_e_threshold)
        {
          continue;
        }

        unsigned int towerkey = towers->encode_key(channel);
        unsigned int ieta_bin = towers->getTowerEtaBin(towerkey);
        unsigned int iphi_bin = towers->getTowerPhiBin(towerkey);
        const RawTowerDefs::keytype key = RawTowerDefs::encode_towerid(
            RawTowerDefs::convert_name_to_caloid("CEMC"), ieta_bin, iphi_bin);
        RawTowerGeom* tower_geom = towergeom->get_tower_geometry(key);
        if (!tower_geom)
        {
          std::cout << PHWHERE << " ERROR: Can't find tower geometry for this tower hit: ";
          tower->identify();
          exit(-1);
        }

        // std::cout<<"Tower ID = "<<tower->get_id()<<" for bin(j,k)= "<<tower->get_bineta()<<","<<tower->get_binphi()<<std::endl; //Added by Barak
        const float towerid = key;
        const float ieta = ieta_bin;
        const float iphi = iphi_bin;
        const float eta = tower_geom->get_eta();
        const float phi = tower_geom->get_phi();
        const float e = tower->get_energy();
        const float x = tower_geom->get_center_x();
        const float y = tower_geom->get_center_y();
        const float z = tower_geom->get_center_z();

        // Added by Barak
        _towerID_debug = towerid;
        _ieta_debug = ieta;
        _iphi_debug = iphi;
        _eta_debug = eta;
        _phi_debug = phi;
        _e_debug = e;
        _x_debug = x;
        _y_debug = y;
        _z_debug = z;

        PHG4Particle* primary = towereval->max_truth_primary_particle_by_energy(tower);

        float gparticleID = NAN;
        float gflavor = NAN;
        float gnhits = NAN;
        float gpx = NAN;
        float gpy = NAN;
        float gpz = NAN;
        float ge = NAN;

        float gpt = NAN;
        float geta = NAN;
        float gphi = NAN;

        float gvx = NAN;
        float gvy = NAN;
        float gvz = NAN;

        float gembed = NAN;
        float gedep = NAN;

        float efromtruth = NAN;

        if (primary)
        {
          gparticleID = primary->get_track_id();
          gflavor = primary->get_pid();

          PHG4Shower* shower = trutheval->get_primary_shower(primary);
          if (shower)
          {
            gnhits = shower->get_nhits(trutheval->get_caloid());
          }
          else
          {
            gnhits = 0.0;
          }
          gpx = primary->get_px();
          gpy = primary->get_py();
          gpz = primary->get_pz();
          ge = primary->get_e();

          gpt = std::sqrt(gpx * gpx + gpy * gpy);
          if (gpt != 0.0)
          {
            geta = std::asinh(gpz / gpt);
          }
          gphi = std::atan2(gpy, gpx);

          PHG4VtxPoint* vtx = trutheval->get_vertex(primary);

          if (vtx)
          {
            gvx = vtx->get_x();
            gvy = vtx->get_y();
            gvz = vtx->get_z();
          }

          gembed = trutheval->get_embed(primary);
          gedep = trutheval->get_shower_energy_deposit(primary);

          efromtruth = towereval->get_energy_contribution(tower, primary);
        }

        float tower_data[] = {(float) _ievent,
                              towerid,
                              ieta,
                              iphi,
                              eta,
                              phi,
                              e,
                              x,
                              y,
                              z,
                              gparticleID,
                              gflavor,
                              gnhits,
                              geta,
                              gphi,
                              ge,
                              gpt,
                              gvx,
                              gvy,
                              gvz,
                              gembed,
                              gedep,
                              efromtruth};

        _ntp_tower->Fill(tower_data);
        _tower_debug->Fill();  // Added by Barak (see above for explanation)
      }
    }
  }
  

  //------------------------
  // fill the Cluster NTuple
  //------------------------

  if (_do_cluster_eval)
  {
    if (Verbosity() > 1)
    {
      std::cout << "CaloEvaluator::filling gcluster ntuple..." << std::endl;
    }

    GlobalVertexMap* vertexmap = findNode::getClass<GlobalVertexMap>(topNode, "GlobalVertexMap");

    std::string clusternode = "CLUSTER_" + _caloname;
    if (_use_towerinfo)
    {
      clusternode = "CLUSTER_CALIB_" + _caloname;
    }
    RawClusterContainer* clusters = findNode::getClass<RawClusterContainer>(topNode, clusternode.c_str());
    if (!clusters)
    {
      std::cout << PHWHERE << " ERROR: Can't find " << clusternode << std::endl;
      exit(-1);
    }

    // for every cluster

    for (const auto& iterator : clusters->getClustersMap())
    {
      RawCluster* cluster = iterator.second;

      //    for (unsigned int icluster = 0; icluster < clusters->size(); icluster++)
      //    {
      //      RawCluster* cluster = clusters->getCluster(icluster);

      if (cluster->get_energy() < _reco_e_threshold)
      {
        continue;
      }

      float clusterID = cluster->get_id();
      float ntowers = cluster->getNTowers();
      float x = cluster->get_x();
      float y = cluster->get_y();
      float z = cluster->get_z();
      float eta = NAN;
      float phi = cluster->get_phi();
      float e = cluster->get_energy();

      // require vertex for cluster eta calculation
      if (vertexmap)
      {
        if (!vertexmap->empty())
        {
          GlobalVertex* vertex = (vertexmap->begin()->second);

          eta =
              RawClusterUtility::GetPseudorapidity(
                  *cluster,
                  CLHEP::Hep3Vector(vertex->get_x(), vertex->get_y(), vertex->get_z()));
        }
      }

      PHG4Particle* primary = clustereval->max_truth_primary_particle_by_energy(cluster);

      float gparticleID = NAN;
      float gflavor = NAN;

      float gnhits = NAN;
      float gpx = NAN;
      float gpy = NAN;
      float gpz = NAN;
      float ge = NAN;

      float gpt = NAN;
      float geta = NAN;
      float gphi = NAN;

      float gvx = NAN;
      float gvy = NAN;
      float gvz = NAN;

      float gembed = NAN;
      float gedep = NAN;

      float efromtruth = NAN;

      if (primary)
      {
        gparticleID = primary->get_track_id();
        gflavor = primary->get_pid();

        PHG4Shower* shower = trutheval->get_primary_shower(primary);
        if (shower)
        {
          gnhits = shower->get_nhits(trutheval->get_caloid());
        }
        else
        {
          gnhits = 0.0;
        }
        gpx = primary->get_px();
        gpy = primary->get_py();
        gpz = primary->get_pz();
        ge = primary->get_e();

        gpt = std::sqrt(gpx * gpx + gpy * gpy);
        if (gpt != 0.0)
        {
          geta = std::asinh(gpz / gpt);
        }
        gphi = std::atan2(gpy, gpx);

        PHG4VtxPoint* vtx = trutheval->get_vertex(primary);

        if (vtx)
        {
          gvx = vtx->get_x();
          gvy = vtx->get_y();
          gvz = vtx->get_z();
        }

        gembed = trutheval->get_embed(primary);
        gedep = trutheval->get_shower_energy_deposit(primary);

        efromtruth = clustereval->get_energy_contribution(cluster,
                                                          primary);
      }

      float cluster_data[] = {(float) _ievent,
                              clusterID,
                              ntowers,
                              eta,
                              x,
                              y,
                              z,
                              phi,
                              e,
                              gparticleID,
                              gflavor,
                              gnhits,
                              geta,
                              gphi,
                              ge,
                              gpt,
                              gvx,
                              gvy,
                              gvz,
                              gembed,
                              gedep,
                              efromtruth};

      _ntp_cluster->Fill(cluster_data);
    }
  }

  return;
}
