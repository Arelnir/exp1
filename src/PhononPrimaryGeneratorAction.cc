/***********************************************************************\
 * This software is licensed under the terms of the GNU General Public *
 * License version 3 or later. See G4CMP/LICENSE for the full license. *
\***********************************************************************/

/// \file exoticphysics/phonon/src/PhononPrimaryGeneratorAction.cc
/// \brief Implementation of the PhononPrimaryGeneratorAction class
//
// $Id: e75f788b103aef810361fad30f75077829192c13 $
//
// 20140519  Allow the user to specify phonon type by name in macro; if
//	     "geantino" is set, use random generator to select.
//
// Modified: Replaced G4ParticleGun with G4GeneralParticleSource to support
//           /gps/ commands (volume source, etc.)

#include "PhononPrimaryGeneratorAction.hh"

#include "G4Event.hh"
#include "G4GeneralParticleSource.hh"   // <-- 新增
#include "G4Geantino.hh"
#include "G4ParticleGun.hh"             // 可保留，但不再使用
#include "G4RandomDirection.hh"
#include "G4PhononTransFast.hh"
#include "G4PhononTransSlow.hh"
#include "G4PhononLong.hh"
#include "G4SystemOfUnits.hh"

using namespace std;

PhononPrimaryGeneratorAction::PhononPrimaryGeneratorAction() { 
  // 用 GPS 替代原来的 ParticleGun
  fGPS = new G4GeneralParticleSource();

  // 以下可选：设置默认粒子为 geantino，以便宏中没有指定粒子时不会报错。
  // 实际使用时，用户需在宏中用 /gps/particle 明确设定，或保留此默认值。
  fGPS->SetParticleDefinition(G4Geantino::Definition());
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

PhononPrimaryGeneratorAction::~PhononPrimaryGeneratorAction() {
  delete fGPS;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

void PhononPrimaryGeneratorAction::GeneratePrimaries(G4Event* anEvent) {
  // 直接委托给 GPS 生成初级顶点，忽略旧的随机极化逻辑
  fGPS->GeneratePrimaryVertex(anEvent);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....