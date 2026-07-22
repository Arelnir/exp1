/***********************************************************************\
 * This software is licensed under the terms of the GNU General Public *
 * License version 3 or later. See G4CMP/LICENSE for the full license. *
\***********************************************************************/

#include "PhononDetectorConstruction.hh"
#include "PhononSensitivity.hh"
#include "G4CMPLogicalBorderSurface.hh"
#include "G4CMPPhononElectrode.hh"
#include "G4CMPSurfaceProperty.hh"
#include "G4CMPUtils.hh"
#include "G4Box.hh"
#include "G4Colour.hh"
#include "G4GeometryManager.hh"
#include "G4LatticeLogical.hh"
#include "G4LatticeManager.hh"
#include "G4LatticePhysical.hh"
#include "G4LogicalVolume.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4Material.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4RunManager.hh"
#include "G4SDManager.hh"
#include "G4SolidStore.hh"
#include "G4SystemOfUnits.hh"
#include "G4TransportationManager.hh"
#include "G4Tubs.hh"
#include "G4UserLimits.hh"
#include "G4VisAttributes.hh"
#include "G4ExtrudedSolid.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

PhononDetectorConstruction::PhononDetectorConstruction()
  : fLiquidHelium(0), fSilicon(0), fAluminum(0),
    fWorldPhys(0), fElectrodeSurfProp(0), wallSurfProp(0),
    electrodeSensitivity(0), fConstructed(false) {;}

PhononDetectorConstruction::~PhononDetectorConstruction() {
  delete fElectrodeSurfProp;
  delete wallSurfProp;
}

G4VPhysicalVolume* PhononDetectorConstruction::Construct() {
  if (fConstructed) {
    if (!G4RunManager::IfGeometryHasBeenDestroyed()) {
      G4GeometryManager::GetInstance()->OpenGeometry();
      G4PhysicalVolumeStore::GetInstance()->Clean();
      G4LogicalVolumeStore::GetInstance()->Clean();
      G4SolidStore::GetInstance()->Clean();
    }
    G4LatticeManager::GetLatticeManager()->Reset();
    G4CMPLogicalBorderSurface::CleanSurfaceTable();
  }
  DefineMaterials();
  SetupGeometry();
  fConstructed = true;
  return fWorldPhys;
}

void PhononDetectorConstruction::DefineMaterials() {
  G4NistManager* nistManager = G4NistManager::Instance();
  fLiquidHelium = nistManager->FindOrBuildMaterial("G4_AIR");
  fSilicon      = nistManager->FindOrBuildMaterial("G4_Si");
  fAluminum     = nistManager->FindOrBuildMaterial("G4_Al");
}

void PhononDetectorConstruction::SetupGeometry() {
  // ---------- 世界体积 ----------
  G4double worldXY = 2.5 * cm;
  G4double worldZ  = 1.0 * cm;
  G4VSolid* worldSolid = new G4Box("World", worldXY/2., worldXY/2., worldZ/2.);
  G4LogicalVolume* worldLV = new G4LogicalVolume(worldSolid, fLiquidHelium, "World");
  worldLV->SetUserLimits(new G4UserLimits(10*mm, DBL_MAX, DBL_MAX, 0, 0));
  fWorldPhys = new G4PVPlacement(0, G4ThreeVector(), worldLV, "World", 0, false, 0);

  // ---------- 硅晶体 ----------
  G4double siX = 10000. * um;
  G4double siY = 3333.33 * um;
  G4double siZ = 675.0 * um;
  G4VSolid* siSolid = new G4Box("SiCrystal", siX/2., siY/2., siZ/2.);
  G4LogicalVolume* siLV = new G4LogicalVolume(siSolid, fSilicon, "SiCrystal");
  G4VPhysicalVolume* siPhys = new G4PVPlacement(0, G4ThreeVector(0,0,0),
                                                siLV, "SiCrystal", worldLV, false, 0);

  // ---------- 晶格 ----------
  G4LatticeManager* LM = G4LatticeManager::GetLatticeManager();
  G4LatticeLogical* SiLog = LM->LoadLattice(fSilicon, "Si");
  G4LatticePhysical* SiPhys = new G4LatticePhysical(SiLog);
  SiPhys->SetMillerOrientation(1,0,0);
  LM->RegisterLattice(siPhys, SiPhys);

  // ---------- 可视化 ----------
  worldLV->SetVisAttributes(G4VisAttributes::GetInvisible());
  G4VisAttributes* siVisAtt = new G4VisAttributes(G4Colour(0.50, 0.80, 0.60));
  siVisAtt->SetVisibility(true);
  siLV->SetVisAttributes(siVisAtt);

  // ---------- 表面属性 ----------
  if (!fConstructed) {
    wallSurfProp = new G4CMPSurfaceProperty("SiWall",
        0.0, 1.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 1.0);
    new G4CMPLogicalBorderSurface("SiSide", siPhys, fWorldPhys, wallSurfProp);

    fElectrodeSurfProp = new G4CMPSurfaceProperty("ElectrodeSurf",
        1.0, 0.0, 0.0, 0.0,
        0.94495, 0.05505, 0.0, 0.0,
        1.0, 0.0);
    
    // ---------- 频率依赖的散射系数 (当前设为全吸收) ----------
    const G4double GHz = 1e9 * hertz;

    // 非谐衰减、漫反射、镜面反射的多项式系数（全部为零 → 无反射，全吸收）
    const std::vector<G4double> anhCoeffs  = {};  // 或 {0.0}
    const std::vector<G4double> diffCoeffs = {};
    const std::vector<G4double> specCoeffs = {};

    // 截止频率：设为零表示所有频率均不发生散射
    const G4double anhCutoff  = 0.;
    const G4double reflCutoff = 0.;

    fElectrodeSurfProp->AddScatteringProperties(anhCutoff, reflCutoff,
                                                anhCoeffs, diffCoeffs, specCoeffs,
                                                GHz, GHz, GHz);
        
    AttachPhononSensor(fElectrodeSurfProp);
  }

  G4double elecThick = 0.1 * um;
  G4double siHalfZ = siZ / 2.;

  // ================= CPW =================
  G4double cpwHalfX = 8520./2. * um;
  G4double cpwHalfY = 54./2. * um;
  G4double cpwHalfZ = elecThick / 2.;
  G4VSolid* cpwSolid = new G4Box("CPW", cpwHalfX, cpwHalfY, cpwHalfZ);
  G4LogicalVolume* cpwLV = new G4LogicalVolume(cpwSolid, fAluminum, "CPW_LV");

  G4double cpwCenterY = (-1666.665*um + 2156.33*um) + cpwHalfY;
  G4double cpwZ = siHalfZ + cpwHalfZ;
  G4VPhysicalVolume* cpwPhys = new G4PVPlacement(0,
      G4ThreeVector(0., cpwCenterY, cpwZ), cpwLV, "CPW", worldLV, false, 0);
  new G4CMPLogicalBorderSurface("Si_CPW", siPhys, cpwPhys, fElectrodeSurfProp);
  new G4CMPLogicalBorderSurface("CPW_Si", cpwPhys, siPhys, fElectrodeSurfProp);
  G4VisAttributes* copperVisAtt = new G4VisAttributes(G4Colour(1.0, 0.6, 0.2));
  cpwLV->SetVisAttributes(copperVisAtt);

  // ================= IDC =================
  G4double idcMetalHalfX = 74.175/2. * um;
  G4double idcMetalHalfY = 409./2. * um;
  G4double idcHalfZ = elecThick / 2.;
  G4VSolid* idcMetalSolid = new G4Box("IDCmetal", idcMetalHalfX, idcMetalHalfY, idcHalfZ);
  G4LogicalVolume* idcMetalLV = new G4LogicalVolume(idcMetalSolid, fAluminum, "IDCmetal_LV");

  G4double idcYcenter = (cpwCenterY - cpwHalfY) - 3.*um - idcMetalHalfY;
  G4double idcZcenter = siHalfZ + idcHalfZ;
  G4double L1_x = -3681.044 * um;
  G4double L2_x = -3550.000 * um;
  G4double L3_x = -3418.956 * um;
  G4double R1_x = 3418.956 * um;
  G4double R2_x = 3550.000 * um;
  G4double R3_x = 3681.044 * um;

  G4VPhysicalVolume* pv;
  auto placeIDC = [&](G4double x, const char* name, G4int copyNo) {
      pv = new G4PVPlacement(0, G4ThreeVector(x, idcYcenter, idcZcenter),
                             idcMetalLV, name, worldLV, false, copyNo);
      new G4CMPLogicalBorderSurface("Si_"+G4String(name), siPhys, pv, fElectrodeSurfProp);
      new G4CMPLogicalBorderSurface(G4String(name)+"_Si", pv, siPhys, fElectrodeSurfProp);
  };
  placeIDC(L1_x, "IDC_L1", 0); placeIDC(L2_x, "IDC_L2", 1); placeIDC(L3_x, "IDC_L3", 2);
  placeIDC(R1_x, "IDC_R1", 3); placeIDC(R2_x, "IDC_R2", 4); placeIDC(R3_x, "IDC_R3", 5);
  idcMetalLV->SetVisAttributes(copperVisAtt);

  // ================= 灵敏探测器 =================
  G4SDManager* SDman = G4SDManager::GetSDMpointer();
  if (!electrodeSensitivity) {
    electrodeSensitivity = new PhononSensitivity("PhononElectrode");
    SDman->AddNewDetector(electrodeSensitivity);
    siLV->SetSensitiveDetector(electrodeSensitivity);
  }

  // ================= Resonator =================
  G4double resHalfZ = elecThick / 2.;
  G4double resZcenter = siHalfZ + resHalfZ;
  G4double barHalfX = 45.8225/2. * um, barHalfY = 348./2. * um;
  G4VSolid* barSolid = new G4Box("ResBar", barHalfX, barHalfY, resHalfZ);
  G4LogicalVolume* barLV = new G4LogicalVolume(barSolid, fAluminum, "ResBar_LV");
  //barLV->SetSensitiveDetector(electrodeSensitivity);
  G4double resX_left = -3550.0 * um, resX_right = 3550.0 * um;
  G4double resBarYcenter = (77.665*um - 174.*um);
  G4double leftBar1_X = resX_left - 52.58875 * um, leftBar2_X = resX_left + 52.58875 * um;
  G4double rightBar1_X = resX_right - 52.58875 * um, rightBar2_X = resX_right + 52.58875 * um;

  auto placeBar = [&](G4double x, G4double y, const char* name, G4int copyNo) {
      G4VPhysicalVolume* pvBar = new G4PVPlacement(0, G4ThreeVector(x, y, resZcenter),
                                                   barLV, name, worldLV, false, copyNo);
      new G4CMPLogicalBorderSurface("Si_"+G4String(name), siPhys, pvBar, fElectrodeSurfProp);
      new G4CMPLogicalBorderSurface(G4String(name)+"_Si", pvBar, siPhys, fElectrodeSurfProp);
  };
  placeBar(leftBar1_X, resBarYcenter, "ResL_Bar1", 100);
  placeBar(leftBar2_X, resBarYcenter, "ResL_Bar2", 101);
  placeBar(rightBar1_X, resBarYcenter, "ResR_Bar1", 102);
  placeBar(rightBar2_X, resBarYcenter, "ResR_Bar2", 103);

  G4double stripHalfX = 8./2. * um, stripHalfY = 216./2. * um;
  G4VSolid* stripSolid = new G4Box("ResStrip", stripHalfX, stripHalfY, resHalfZ);
  G4LogicalVolume* stripLV = new G4LogicalVolume(stripSolid, fAluminum, "ResStrip_LV");
  //stripLV->SetSensitiveDetector(electrodeSensitivity);
  G4double stripYcenter = (-270.335*um - 108.*um);
  auto placeStrip = [&](G4double x, G4double y, const char* name, G4int copyNo) {
      G4VPhysicalVolume* pvStrip = new G4PVPlacement(0, G4ThreeVector(x, y, resZcenter),
                                                     stripLV, name, worldLV, false, copyNo);
      new G4CMPLogicalBorderSurface("Si_"+G4String(name), siPhys, pvStrip, fElectrodeSurfProp);
      new G4CMPLogicalBorderSurface(G4String(name)+"_Si", pvStrip, siPhys, fElectrodeSurfProp);
  };
  placeStrip(resX_left, stripYcenter, "ResL_Strip", 200);
  placeStrip(resX_right, stripYcenter, "ResR_Strip", 201);

  G4double halfDiscRadius = 25. * um;
  G4VSolid* halfDiscSolid = new G4Tubs("ResHalfDisc", 0., halfDiscRadius, resHalfZ, 180.*deg, 180.*deg);
  G4LogicalVolume* halfDiscLV = new G4LogicalVolume(halfDiscSolid, fAluminum, "ResHalfDisc_LV");
  //halfDiscLV->SetSensitiveDetector(electrodeSensitivity);
  G4double discYcenter = (-270.335*um - 216.*um);
  auto placeHalfDisc = [&](G4double x, G4double y, const char* name, G4int copyNo) {
      G4VPhysicalVolume* pvDisc = new G4PVPlacement(0, G4ThreeVector(x, y, resZcenter),
                                                    halfDiscLV, name, worldLV, false, copyNo);
      new G4CMPLogicalBorderSurface("Si_"+G4String(name), siPhys, pvDisc, fElectrodeSurfProp);
      new G4CMPLogicalBorderSurface(G4String(name)+"_Si", pvDisc, siPhys, fElectrodeSurfProp);
  };
  placeHalfDisc(resX_left, discYcenter, "ResL_Disc", 300);
  placeHalfDisc(resX_right, discYcenter, "ResR_Disc", 301);

  G4VisAttributes* cyanVisAtt = new G4VisAttributes(G4Colour(0.0, 1.0, 1.0));
  barLV->SetVisAttributes(cyanVisAtt);
  stripLV->SetVisAttributes(cyanVisAtt);
  halfDiscLV->SetVisAttributes(cyanVisAtt);

  // ================= CPW 端口 (梯形 + 矩形) =================
  G4double portHalfZ = elecThick / 2.;
  G4double portZcenter = siHalfZ + portHalfZ;

  std::vector<G4TwoVector> trapezoidLeft = {
      G4TwoVector(-4550.*um, -83.335*um),
      G4TwoVector(-4550.*um, 1116.665*um),
      G4TwoVector(-4260.*um, 543.665*um),
      G4TwoVector(-4260.*um, 489.665*um)
  };
  std::vector<G4TwoVector> trapezoidRight = {
      G4TwoVector(4550.*um, -83.335*um),
      G4TwoVector(4550.*um, 1116.665*um),
      G4TwoVector(4260.*um, 543.665*um),
      G4TwoVector(4260.*um, 489.665*um)
  };

  // 定义 Z 段（上下对称）
  G4ExtrudedSolid::ZSection zsecArr[2] = {
      G4ExtrudedSolid::ZSection(-portHalfZ, G4TwoVector(0,0), 1.0),
      G4ExtrudedSolid::ZSection( portHalfZ, G4TwoVector(0,0), 1.0)
  };
  std::vector<G4ExtrudedSolid::ZSection> zsec(zsecArr, zsecArr+2);

  G4VSolid* trapLeftSolid = new G4ExtrudedSolid("CPW_PortTrapLeft", trapezoidLeft, zsec);
  G4VSolid* trapRightSolid = new G4ExtrudedSolid("CPW_PortTrapRight", trapezoidRight, zsec);
  G4LogicalVolume* trapLeftLV = new G4LogicalVolume(trapLeftSolid, fAluminum, "CPW_PortTrapLeftLV");
  G4LogicalVolume* trapRightLV = new G4LogicalVolume(trapRightSolid, fAluminum, "CPW_PortTrapRightLV");

  G4double rectHalfX = 200./2. * um, rectHalfY = 1200./2. * um;
  G4VSolid* rectSolid = new G4Box("CPW_PortRect", rectHalfX, rectHalfY, portHalfZ);
  G4LogicalVolume* rectLeftLV = new G4LogicalVolume(rectSolid, fAluminum, "CPW_PortRectLeftLV");
  G4LogicalVolume* rectRightLV = new G4LogicalVolume(rectSolid, fAluminum, "CPW_PortRectRightLV");

  G4VPhysicalVolume* pvPort;
  pvPort = new G4PVPlacement(0, G4ThreeVector(0,0,portZcenter), trapLeftLV, "CPW_PortTrapLeft", worldLV, false, 600);
  new G4CMPLogicalBorderSurface("Si_TrapLeft", siPhys, pvPort, fElectrodeSurfProp);
  new G4CMPLogicalBorderSurface("TrapLeft_Si", pvPort, siPhys, fElectrodeSurfProp);
  pvPort = new G4PVPlacement(0, G4ThreeVector(0,0,portZcenter), trapRightLV, "CPW_PortTrapRight", worldLV, false, 601);
  new G4CMPLogicalBorderSurface("Si_TrapRight", siPhys, pvPort, fElectrodeSurfProp);
  new G4CMPLogicalBorderSurface("TrapRight_Si", pvPort, siPhys, fElectrodeSurfProp);

  pvPort = new G4PVPlacement(0, G4ThreeVector(-4650.*um, 516.665*um, portZcenter),
                             rectLeftLV, "CPW_PortRectLeft", worldLV, false, 602);
  new G4CMPLogicalBorderSurface("Si_RectLeft", siPhys, pvPort, fElectrodeSurfProp);
  new G4CMPLogicalBorderSurface("RectLeft_Si", pvPort, siPhys, fElectrodeSurfProp);
  pvPort = new G4PVPlacement(0, G4ThreeVector(4650.*um, 516.665*um, portZcenter),
                             rectRightLV, "CPW_PortRectRight", worldLV, false, 603);
  new G4CMPLogicalBorderSurface("Si_RectRight", siPhys, pvPort, fElectrodeSurfProp);
  new G4CMPLogicalBorderSurface("RectRight_Si", pvPort, siPhys, fElectrodeSurfProp);

  trapLeftLV->SetVisAttributes(copperVisAtt);
  trapRightLV->SetVisAttributes(copperVisAtt);
  rectLeftLV->SetVisAttributes(copperVisAtt);
  rectRightLV->SetVisAttributes(copperVisAtt);

  // ================= 边缘固定电极 =================
  G4double edgePadHalfX = 400./2. * um, edgePadHalfY = 100./2. * um;
  G4double edgePadHalfZ = elecThick / 2.;
  G4VSolid* edgePadSolid = new G4Box("EdgePad", edgePadHalfX, edgePadHalfY, edgePadHalfZ);
  G4LogicalVolume* edgePadLV = new G4LogicalVolume(edgePadSolid, fAluminum, "EdgePad_LV");
  G4double edgeX[3] = { -2375.*um, 0.*um, 2375.*um };
  G4double edgeY_top = 1366.665 * um, edgeY_bot = -1366.665 * um;
  G4double edgeZ = siHalfZ + edgePadHalfZ;
  G4VPhysicalVolume* pvEdge;
  for (int i=0; i<3; ++i) {
      pvEdge = new G4PVPlacement(0, G4ThreeVector(edgeX[i], edgeY_top, edgeZ),
                                 edgePadLV, "EdgePad_Top", worldLV, false, 700+i);
      new G4CMPLogicalBorderSurface("Si_EdgeTop"+G4String(std::to_string(i)), siPhys, pvEdge, fElectrodeSurfProp);
      new G4CMPLogicalBorderSurface("EdgeTop"+G4String(std::to_string(i))+"_Si", pvEdge, siPhys, fElectrodeSurfProp);
      pvEdge = new G4PVPlacement(0, G4ThreeVector(edgeX[i], edgeY_bot, edgeZ),
                                 edgePadLV, "EdgePad_Bot", worldLV, false, 710+i);
      new G4CMPLogicalBorderSurface("Si_EdgeBot"+G4String(std::to_string(i)), siPhys, pvEdge, fElectrodeSurfProp);
      new G4CMPLogicalBorderSurface("EdgeBot"+G4String(std::to_string(i))+"_Si", pvEdge, siPhys, fElectrodeSurfProp);
  }
  G4VisAttributes* purpleVisAtt = new G4VisAttributes(G4Colour(0.7, 0.4, 0.9));
  edgePadLV->SetVisAttributes(purpleVisAtt);
    // ==================== 临时：验证能量守恒（添加所有电极的灵敏探测器） ====================
  // 使用时取消注释下面这些行；验证完毕后重新注释或删除即可恢复原始模式。
  cpwLV->SetSensitiveDetector(electrodeSensitivity);
  idcMetalLV->SetSensitiveDetector(electrodeSensitivity);
  trapLeftLV->SetSensitiveDetector(electrodeSensitivity);
  trapRightLV->SetSensitiveDetector(electrodeSensitivity);
  rectLeftLV->SetSensitiveDetector(electrodeSensitivity);
  rectRightLV->SetSensitiveDetector(electrodeSensitivity);
  edgePadLV->SetSensitiveDetector(electrodeSensitivity);
  // 注意：Resonator 各部分已经在原代码中设置了灵敏探测器，无需重复。
  // ==================== 临时结束 ====================
}

void PhononDetectorConstruction::AttachPhononSensor(G4CMPSurfaceProperty *surfProp) {
  if (!surfProp) return;
  auto sensorProp = surfProp->GetPhononMaterialPropertiesTablePointer();
  G4CMP::UpdateMPT(sensorProp, "filmAbsorption", 1.0);
  G4CMP::UpdateMPT(sensorProp, "filmThickness", 100.*nm);
  G4CMP::UpdateMPT(sensorProp, "gapEnergy", 173.715e-6*eV);
  G4CMP::UpdateMPT(sensorProp, "lowQPLimit", 3.);
  G4CMP::UpdateMPT(sensorProp, "phononLifetime", 242.*ps);
  G4CMP::UpdateMPT(sensorProp, "phononLifetimeSlope", 0.29);
  G4CMP::UpdateMPT(sensorProp, "vSound", 3.26*km/s);
  G4CMP::UpdateMPT(sensorProp, "subgapAbsorption", 0.0);
  surfProp->SetPhononElectrode(new G4CMPPhononElectrode);
}