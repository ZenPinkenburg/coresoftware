#include "PHG4IHCalDetector.h"

#include "PHG4IHCalDisplayAction.h"

#include <g4detectors/PHG4HcalDefs.h>
#include <g4detectors/PHG4DetectorSubsystem.h>

#include <phparameter/PHParameters.h>

#include <g4gdml/PHG4GDMLConfig.hh>
#include <g4gdml/PHG4GDMLUtility.hh>

#include <calobase/RawTowerDefs.h>           // for convert_name_...
#include <calobase/RawTowerGeom.h>           // for RawTowerGeom
#include <calobase/RawTowerGeomContainer.h>  // for RawTowerGeomC...
#include <calobase/RawTowerGeomContainer_Cylinderv1.h>
#include <calobase/RawTowerGeomv1.h>

#include <g4main/PHG4Detector.h>
#include <g4main/PHG4DisplayAction.h>
#include <g4main/PHG4Subsystem.h>
#include <g4main/PHG4Utils.h>

#include <ffamodules/CDBInterface.h>

#include <phool/PHCompositeNode.h>
#include <phool/PHIODataNode.h>
#include <phool/PHNode.h>  // for PHNode
#include <phool/PHNodeIterator.h>
#include <phool/PHObject.h>  // for PHObject
#include <phool/getClass.h>
#include <phool/phool.h>
#include <phool/recoConsts.h>


#include <TSystem.h>

#include <Geant4/G4AssemblyVolume.hh>
#include <Geant4/G4IonisParamMat.hh>
#include <Geant4/G4LogicalVolume.hh>
#include <Geant4/G4Material.hh>
#include <Geant4/G4MaterialTable.hh>
#include <Geant4/G4PVPlacement.hh>
#include <Geant4/G4RotationMatrix.hh>
#include <Geant4/G4String.hh>
#include <Geant4/G4SystemOfUnits.hh>
#include <Geant4/G4ThreeVector.hh>
#include <Geant4/G4Transform3D.hh>
#include <Geant4/G4Tubs.hh>
#include <Geant4/G4VPhysicalVolume.hh>  // for G4VPhysicalVolume
#include <Geant4/G4VSolid.hh>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <Geant4/G4GDMLParser.hh>
#include <Geant4/G4GDMLReadStructure.hh>  // for G4GDMLReadStructure
#pragma GCC diagnostic pop

#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>   // for unique_ptr
#include <utility>  // for pair, make_pair
#include <vector>   // for vector, vector<>::iter...

PHG4IHCalDetector::PHG4IHCalDetector(PHG4Subsystem *subsys, PHCompositeNode *Node, PHParameters *parameters, const std::string &dnam)
  : PHG4Detector(subsys, Node, dnam)
  , m_DisplayAction(dynamic_cast<PHG4IHCalDisplayAction *>(subsys->GetDisplayAction()))
  , m_Params(parameters)
  , m_InnerRadius(m_Params->get_double_param("inner_radius") * cm)
  , m_OuterRadius(m_Params->get_double_param("outer_radius") * cm)
  , m_SizeZ(m_Params->get_double_param("size_z") * cm)
  , m_NumScintiPlates(m_Params->get_int_param(PHG4HcalDefs::scipertwr) * m_Params->get_int_param("n_towers"))
  , m_Active(m_Params->get_int_param("active"))
  , m_AbsorberActive(m_Params->get_int_param("absorberactive"))
  , m_GDMPath(m_Params->get_string_param("GDMPath"))
{
  gdml_config = PHG4GDMLUtility::GetOrMakeConfigNode(Node);
  assert(gdml_config);
// changes in the parameters have to be made here
// otherwise they will not be propagated to the node tree
  if (std::filesystem::path(m_GDMPath).extension() != ".gdml")
  {
    m_GDMPath = CDBInterface::instance()->getUrl(m_GDMPath);
    m_Params->set_string_param("GDMPath",m_GDMPath);
  }
}

PHG4IHCalDetector::~PHG4IHCalDetector()
{
  delete m_ScintiMotherAssembly;
}

//_______________________________________________________________
//_______________________________________________________________
int PHG4IHCalDetector::IsInIHCal(G4VPhysicalVolume *volume) const
{
  if (m_AbsorberActive)
  {
    if (m_SteelAbsorberLogVolSet.find(volume->GetLogicalVolume()) != m_SteelAbsorberLogVolSet.end())
    {
      return -1;
    }
  }
  if (m_Active)
  {
    if (m_ScintiTileLogVolSet.find(volume->GetLogicalVolume()) != m_ScintiTileLogVolSet.end())
    {
      return 1;
    }
  }
  return 0;
}

// Construct the envelope and the call the
// actual inner hcal construction
void PHG4IHCalDetector::ConstructMe(G4LogicalVolume *logicWorld)
{
  recoConsts *rc = recoConsts::instance();
  G4Material *worldmat = GetDetectorMaterial(rc->get_StringFlag("WorldMaterial"));
  G4VSolid *hcal_envelope_cylinder = new G4Tubs("IHCal_envelope_solid", m_InnerRadius, m_OuterRadius, m_SizeZ / 2., 0, 2 * M_PI);
  m_VolumeEnvelope = hcal_envelope_cylinder->GetCubicVolume();
  G4LogicalVolume *hcal_envelope_log = new G4LogicalVolume(hcal_envelope_cylinder, worldmat, "Hcal_envelope", nullptr, nullptr, nullptr);

  G4RotationMatrix hcal_rotm;
  hcal_rotm.rotateX(m_Params->get_double_param("rot_x") * deg);
  hcal_rotm.rotateY(m_Params->get_double_param("rot_y") * deg);
  hcal_rotm.rotateZ(m_Params->get_double_param("rot_z") * deg);
  G4VPhysicalVolume *mothervol = new G4PVPlacement(G4Transform3D(hcal_rotm, G4ThreeVector(m_Params->get_double_param("place_x") * cm, m_Params->get_double_param("place_y") * cm, m_Params->get_double_param("place_z") * cm)), hcal_envelope_log, "IHCalEnvelope", logicWorld, false, false, OverlapCheck());
  m_DisplayAction->SetMyTopVolume(mothervol);
  ConstructIHCal(hcal_envelope_log);

  // disable GDML export for HCal geometries for memory saving and compatibility issues
  assert(gdml_config);
  gdml_config->exclude_physical_vol(mothervol);
  gdml_config->exclude_logical_vol(hcal_envelope_log);

  const G4MaterialTable *mtable = G4Material::GetMaterialTable();
  int nMaterials = G4Material::GetNumberOfMaterials();
  for (auto i = 0; i < nMaterials; ++i)
  {
    const G4Material *mat = (*mtable)[i];
    if (mat->GetName() == "Uniplast_scintillator")
    {
      if ((mat->GetIonisation()->GetBirksConstant()) == 0)
      {
        mat->GetIonisation()->SetBirksConstant(m_Params->get_double_param("Birk_const"));
      }
    }
  }
  if (!m_Params->get_int_param("saveg4hit"))
  {
    AddGeometryNode();
  }
  return;
}

int PHG4IHCalDetector::ConstructIHCal(G4LogicalVolume *hcalenvelope)
{
  // import the staves from the geometry file
  std::unique_ptr<G4GDMLReadStructure> reader(new G4GDMLReadStructure());
  G4GDMLParser gdmlParser(reader.get());
  gdmlParser.SetOverlapCheck(OverlapCheck());
  if (! std::filesystem::exists(m_GDMPath))
  {
    std::cout << PHWHERE << " Inner HCal gdml file " << m_GDMPath << " not found" << std::endl;
    gSystem->Exit(1);
    exit(1);
  }
  gdmlParser.Read(m_GDMPath, false);

  G4AssemblyVolume *abs_asym = reader->GetAssembly("InnerSector");      // absorber
  m_ScintiMotherAssembly = reader->GetAssembly("InnerTileAssembly90");  // scintillator
  std::vector<G4VPhysicalVolume *>::iterator it = abs_asym->GetVolumesIterator();
  static const unsigned int tilepersec = 24 * 4 * 2;
  for (unsigned int isector = 0; isector < abs_asym->TotalImprintedVolumes(); isector++)
  {
    m_DisplayAction->AddSteelVolume((*it)->GetLogicalVolume());
    m_SteelAbsorberLogVolSet.insert((*it)->GetLogicalVolume());
    hcalenvelope->AddDaughter((*it));
    m_AbsorberPhysVolMap.insert(std::make_pair(*it, isector));
    m_VolumeSteel += (*it)->GetLogicalVolume()->GetSolid()->GetCubicVolume();
    std::vector<G4VPhysicalVolume *>::iterator its = m_ScintiMotherAssembly->GetVolumesIterator();
    unsigned int ioff = isector * tilepersec;
    for (unsigned int j = 0; j < ioff; j++)
    {
      ++its;
    }
    for (unsigned int j = ioff; j < ioff + tilepersec; j++)
    {
      m_DisplayAction->AddScintiVolume((*its)->GetLogicalVolume());
      m_ScintiTileLogVolSet.insert((*its)->GetLogicalVolume());
      hcalenvelope->AddDaughter((*its));
      m_ScintiTilePhysVolMap.insert(std::make_pair(*its, ExtractLayerTowerId(isector, *its)));
      m_VolumeScintillator += (*its)->GetLogicalVolume()->GetSolid()->GetCubicVolume();
      ++its;
    }

    ++it;
  }
  return 0;
}

int PHG4IHCalDetector::ConsistencyCheck() const
{
  // just make sure the parameters make a bit of sense
  if (m_InnerRadius >= m_OuterRadius)
  {
    std::cout << PHWHERE << ": Inner Radius " << m_InnerRadius / cm
              << " cm larger than Outer Radius " << m_OuterRadius / cm
              << " cm" << std::endl;
    gSystem->Exit(1);
  }
  return 0;
}

void PHG4IHCalDetector::Print(const std::string &what) const
{
  std::cout << "Inner Hcal Detector:" << std::endl;
  if (what == "ALL" || what == "VOLUME")
  {
    std::cout << "Volume Envelope: " << m_VolumeEnvelope / cm3 << " cm^3" << std::endl;
    std::cout << "Volume Steel: " << m_VolumeSteel / cm3 << " cm^3" << std::endl;
    std::cout << "Volume Scintillator: " << m_VolumeScintillator / cm3 << " cm^3" << std::endl;
    std::cout << "Volume Air: " << (m_VolumeEnvelope - m_VolumeSteel - m_VolumeScintillator) / cm3 << " cm^3" << std::endl;
  }
  return;
}

std::tuple<int, int, int> PHG4IHCalDetector::GetLayerTowerId(G4VPhysicalVolume *volume) const
{
  auto it = m_ScintiTilePhysVolMap.find(volume);
  if (it != m_ScintiTilePhysVolMap.end())
  {
    return it->second;
  }
  std::cout << "could not locate volume " << volume->GetName()
            << " in Inner Hcal scintillator map" << std::endl;
  gSystem->Exit(1);
  // that's dumb but code checkers do not know that gSystem->Exit()
  // terminates, so using the standard exit() makes them happy
  exit(1);
}

int PHG4IHCalDetector::GetSectorId(G4VPhysicalVolume *volume) const
{
  auto it = m_AbsorberPhysVolMap.find(volume);
  if (it != m_AbsorberPhysVolMap.end())
  {
    return it->second;
  }
  std::cout << "could not locate volume " << volume->GetName()
            << " in Inner Hcal Absorber map" << std::endl;
  gSystem->Exit(1);
  // that's dumb but code checkers do not know that gSystem->Exit()
  // terminates, so using the standard exit() makes them happy
  exit(1);
}

std::tuple<int, int, int> PHG4IHCalDetector::ExtractLayerTowerId(const unsigned int isector, G4VPhysicalVolume *volume)
{
  boost::char_separator<char> sep("_");
  boost::tokenizer<boost::char_separator<char>> tok(volume->GetName(), sep);
  boost::tokenizer<boost::char_separator<char>>::const_iterator tokeniter;
  int layer_id = -1, tower_id = -1;
  for (tokeniter = tok.begin(); tokeniter != tok.end(); ++tokeniter)
  {
    if (*tokeniter == "impr")
    {
      ++tokeniter;
      if (tokeniter != tok.end())
      {
        layer_id = boost::lexical_cast<int>(*tokeniter) / 2;
        layer_id--;
        if (layer_id < 0 || layer_id >= m_NumScintiPlates)
        {
          std::cout << "invalid scintillator row " << layer_id
                    << ", valid range 0 < row < " << m_NumScintiPlates << std::endl;
          gSystem->Exit(1);
        }
      }
      else
      {
        std::cout << PHWHERE << " Error parsing " << volume->GetName()
                  << " for mother volume number " << std::endl;
        gSystem->Exit(1);
      }
      break;
    }
  }
  for (tokeniter = tok.begin(); tokeniter != tok.end(); ++tokeniter)
  {
    if (*tokeniter == "pv")
    {
      ++tokeniter;
      if (tokeniter != tok.end())
      {
        tower_id = boost::lexical_cast<int>(*tokeniter);
      }
    }
  }
  int column = map_towerid(tower_id);
  int row = map_layerid(layer_id);
  return std::make_tuple(isector, row, column);
}

// map gdml tower ids to our columns
int PHG4IHCalDetector::map_towerid(const int tower_id)
{
  // odd id's go in one direction, even id's in the other one
  // this is a shortcut to derive the observed dependency
  // commented out after this code
  int itwr = -1;
  int itmp = tower_id / 2;
  if (tower_id % 2)
  {
    itwr = 12 + itmp;
  }
  else
  {
    itwr = 11 - itmp;
  }
  return itwr;
  // here is the mapping in long form
  // if (tower_id == 23)
  // {
  //   itwr = 0;
  // }
  // else if (tower_id == 21)
  // {
  //   itwr = 1;
  // }
  // else if (tower_id ==19 )
  // {
  //   itwr = 2;
  // }
  // else if (tower_id == 17)
  // {
  //   itwr = 3;
  // }
  // else if (tower_id == 15)
  // {
  //   itwr = 4;
  // }
  // else if (tower_id == 13)
  // {
  //   itwr = 5;
  // }
  // else if (tower_id == 11)
  // {
  //   itwr = 6;
  // }
  // else if (tower_id == 9)
  // {
  //   itwr = 7;
  // }
  // else if (tower_id == 7)
  // {
  //   itwr = 8;
  // }
  // else if (tower_id == 5)
  // {
  //   itwr = 9;
  // }
  // else if (tower_id == 3)
  // {
  //   itwr = 10;
  // }
  // else if (tower_id == 1)
  // {
  //   itwr = 11;
  // }
  // else if (tower_id == 0)
  // {
  //   itwr = 12;
  // }
  // else if (tower_id == 2)
  // {
  //   itwr = 13;
  // }
  // else if (tower_id == 4)
  // {
  //   itwr = 14;
  // }
  // else if (tower_id == 6)
  // {
  //   itwr = 15;
  // }
  // else if (tower_id == 8)
  // {
  //   itwr = 16;
  // }
  // else if (tower_id == 10)
  // {
  //   itwr = 17;
  // }
  // else if (tower_id == 12)
  // {
  //   itwr = 18;
  // }
  // else if (tower_id == 14)
  // {
  //   itwr = 19;
  // }
  // else if (tower_id == 16)
  // {
  //   itwr = 20;
  // }
  // else if (tower_id == 18)
  // {
  //   itwr = 21;
  // }
  // else if (tower_id == 20)
  // {
  //   itwr = 22;
  // }
  // else if (tower_id == 22)
  // {
  //   itwr = 23;
  // }
  // else
  // {
  //   std::cout << PHWHERE << " cannot map tower " << tower_id << std::endl;
  //   gSystem->Exit(1);
  //   exit(1);
  // }
}

int PHG4IHCalDetector::map_layerid(const int layer_id)
{
  int rowid = -1;

  if (layer_id < 188)
  {
    rowid = layer_id + 68;
  }
  else  // (layer_id >= 188)
  {
    rowid = layer_id - 188;
  }
  // shift the row index up by 4
  rowid += 4;
  if (rowid > 255)
  {
    rowid -= 256;
  }

  if (rowid > 255 || rowid < 0)
  {
    std::cout << PHWHERE << " row id out of range: " << rowid << std::endl;
    gSystem->Exit(1);
  }
  return rowid;
}

// This is dulplicated code, we can get rid of it when we have the code to make towergeom for real data reco.
void PHG4IHCalDetector::AddGeometryNode()
{
  PHNodeIterator iter(topNode());
  PHCompositeNode *runNode = dynamic_cast<PHCompositeNode *>(iter.findFirst("PHCompositeNode", "RUN"));
  if (!runNode)
  {
    std::cout << PHWHERE << "Run Node missing, exiting." << std::endl;
    gSystem->Exit(1);
    exit(1);
  }
  PHNodeIterator runIter(runNode);
  PHCompositeNode *RunDetNode = dynamic_cast<PHCompositeNode *>(runIter.findFirst("PHCompositeNode", m_SuperDetector));
  if (!RunDetNode)
  {
    RunDetNode = new PHCompositeNode(m_SuperDetector);
    runNode->addNode(RunDetNode);
  }
  m_TowerGeomNodeName = "TOWERGEOM_" + m_SuperDetector;
  m_RawTowerGeom = findNode::getClass<RawTowerGeomContainer>(topNode(), m_TowerGeomNodeName);
  if (!m_RawTowerGeom)
  {
    m_RawTowerGeom = new RawTowerGeomContainer_Cylinderv1(RawTowerDefs::convert_name_to_caloid(m_SuperDetector));
    PHIODataNode<PHObject> *newNode = new PHIODataNode<PHObject>(m_RawTowerGeom, m_TowerGeomNodeName, "PHObject");
    RunDetNode->addNode(newNode);
  }
  double innerrad = m_Params->get_double_param(PHG4HcalDefs::innerrad);
  double thickness = m_Params->get_double_param(PHG4HcalDefs::outerrad) - innerrad;
  m_RawTowerGeom->set_radius(innerrad);
  m_RawTowerGeom->set_thickness(thickness);
  m_RawTowerGeom->set_phibins(m_Params->get_int_param(PHG4HcalDefs::n_towers));
  m_RawTowerGeom->set_etabins(m_Params->get_int_param("etabins"));
  double geom_ref_radius = innerrad + thickness / 2.;
  double phistart = m_Params->get_double_param("phistart");
  if (!std::isfinite(phistart))
  {
    std::cout << PHWHERE << " phistart is not finite: " << phistart
              << ", exiting now (this will crash anyway)" << std::endl;
    gSystem->Exit(1);
  }
  for (int i = 0; i < m_Params->get_int_param(PHG4HcalDefs::n_towers); i++)
  {
    double phiend = phistart + 2. * M_PI / m_Params->get_int_param(PHG4HcalDefs::n_towers);
    std::pair<double, double> range = std::make_pair(phiend, phistart);
    phistart = phiend;
    int tempi = i + 1;
    if (tempi >= m_Params->get_int_param(PHG4HcalDefs::n_towers))
    {
      tempi -= m_Params->get_int_param(PHG4HcalDefs::n_towers);
    }
    m_RawTowerGeom->set_phibounds(tempi, range);
  }
  double etalowbound = -m_Params->get_double_param("scinti_eta_coverage_neg");
  for (int i = 0; i < m_Params->get_int_param("etabins"); i++)
  {
    // double etahibound = etalowbound + 2.2 / get_int_param("etabins");
    double etahibound = etalowbound +
                        (m_Params->get_double_param("scinti_eta_coverage_neg") + m_Params->get_double_param("scinti_eta_coverage_pos")) / m_Params->get_int_param("etabins");
    std::pair<double, double> range = std::make_pair(etalowbound, etahibound);
    m_RawTowerGeom->set_etabounds(i, range);
    etalowbound = etahibound;
  }
  for (int iphi = 0; iphi < m_RawTowerGeom->get_phibins(); iphi++)
  {
    for (int ieta = 0; ieta < m_RawTowerGeom->get_etabins(); ieta++)
    {
      const RawTowerDefs::keytype key = RawTowerDefs::encode_towerid(RawTowerDefs::convert_name_to_caloid(m_SuperDetector), ieta, iphi);

      const double x(geom_ref_radius * cos(m_RawTowerGeom->get_phicenter(iphi)));
      const double y(geom_ref_radius * sin(m_RawTowerGeom->get_phicenter(iphi)));
      const double z(geom_ref_radius / tan(PHG4Utils::get_theta(m_RawTowerGeom->get_etacenter(ieta))));

      RawTowerGeom *tg = m_RawTowerGeom->get_tower_geometry(key);
      if (tg)
      {
        if (Verbosity() > 0)
        {
          std::cout << "IHCalDetector::InitRun - Tower geometry " << key << " already exists" << std::endl;
        }

        if (fabs(tg->get_center_x() - x) > 1e-4)
        {
          std::cout << "IHCalDetector::InitRun - Fatal Error - duplicated Tower geometry " << key << " with existing x = " << tg->get_center_x() << " and expected x = " << x
                    << std::endl;

          return;
        }
        if (fabs(tg->get_center_y() - y) > 1e-4)
        {
          std::cout << "IHCalDetector::InitRun - Fatal Error - duplicated Tower geometry " << key << " with existing y = " << tg->get_center_y() << " and expected y = " << y
                    << std::endl;
          return;
        }
        if (fabs(tg->get_center_z() - z) > 1e-4)
        {
          std::cout << "IHCalDetector::InitRun - Fatal Error - duplicated Tower geometry " << key << " with existing z= " << tg->get_center_z() << " and expected z = " << z
                    << std::endl;
          return;
        }
      }
      else
      {
        if (Verbosity() > 0)
        {
          std::cout << "IHCalDetector::InitRun - building tower geometry " << key << "" << std::endl;
        }

        tg = new RawTowerGeomv1(key);

        tg->set_center_x(x);
        tg->set_center_y(y);
        tg->set_center_z(z);
        m_RawTowerGeom->add_tower_geometry(tg);
      }
    }
  }
  if (Verbosity() > 0)
  {
    m_RawTowerGeom->identify();
  }
}
