/***********************************************************************\
 * This software is licensed under the terms of the GNU General Public *
 * License version 3 or later. See G4CMP/LICENSE for the full license. *
\***********************************************************************/

#include "PhononSensitivity.hh"
#include "G4CMPElectrodeHit.hh"
#include "G4Event.hh"
#include "G4HCofThisEvent.hh"
#include "G4PhononLong.hh"
#include "G4PhononTransFast.hh"
#include "G4PhononTransSlow.hh"
#include "G4Run.hh"
#include "G4RunManager.hh"
#include "G4SDManager.hh"
#include "G4SystemOfUnits.hh"
#include "PhononConfigManager.hh"
#include <fstream>

PhononSensitivity::PhononSensitivity(G4String name) :
  G4CMPElectrodeSensitivity(name), fileName("") {
  SetOutputFile(PhononConfigManager::GetHitOutput());
}

PhononSensitivity::~PhononSensitivity() {
  if (output.is_open()) output.close();
  if (!output.good()) {
    G4cerr << "Error closing output file, " << fileName << ".\n"
           << "Expect bad things like loss of data.";
  }
}

void PhononSensitivity::EndOfEvent(G4HCofThisEvent* HCE) {
  G4int HCID = G4SDManager::GetSDMpointer()->GetCollectionID(hitsCollection);
  auto* hitCol = static_cast<G4CMPElectrodeHitsCollection*>(HCE->GetHC(HCID));
  std::vector<G4CMPElectrodeHit*>* hitVec = hitCol->GetVector();

  G4RunManager* runMan = G4RunManager::GetRunManager();

  // 有效 Resonator 区域 (单位：mm)
  const double xLeftMin  = -3.775, xLeftMax  = -3.325;   // 左 Resonator
  const double xRightMin =  3.325, xRightMax =  3.775;   // 右 Resonator
  const double yMin = -0.52, yMax = 0.077665;        // Y 范围（IDC下边到半圆下端）

  if (output.good()) {
    for (G4CMPElectrodeHit* hit : *hitVec) {
      double x = hit->GetFinalPosition().x();   // 单位 mm
      double y = hit->GetFinalPosition().y();

      bool inLeft  = (x >= xLeftMin  && x <= xLeftMax  && y >= yMin && y <= yMax);
      bool inRight = (x >= xRightMin && x <= xRightMax && y >= yMin && y <= yMax);
      // ==================== 临时：验证能量守恒（关闭坐标过滤） ====================
      // 验证时注释掉下面 if 语句，让所有电极击中都被记录。
      // if (!inLeft && !inRight) continue; //跳过无效电极
      // ==================== 临时结束 ====================

      output << runMan->GetCurrentRun()->GetRunID() << ','
             << runMan->GetCurrentEvent()->GetEventID() << ','
             << hit->GetTrackID() << ','
             << hit->GetParticleName() << ','
             << hit->GetStartEnergy()/eV << ','
             << hit->GetStartPosition().getX()/m << ','
             << hit->GetStartPosition().getY()/m << ','
             << hit->GetStartPosition().getZ()/m << ','
             << hit->GetStartTime()/ns << ','
             << hit->GetEnergyDeposit()/eV << ','   // FillHit 已正确填充
             << hit->GetWeight() << ','
             << hit->GetFinalPosition().getX()/m << ','
             << hit->GetFinalPosition().getY()/m << ','
             << hit->GetFinalPosition().getZ()/m << ','
             << hit->GetFinalTime()/ns << '\n';
    }
  }
}

void PhononSensitivity::SetOutputFile(const G4String &fn) {
  if (fileName != fn) {
    if (output.is_open()) output.close();
    fileName = fn;
    output.open(fileName, std::ios_base::app);
    if (!output.good()) {
      G4ExceptionDescription msg;
      msg << "Error opening output file " << fileName;
      G4Exception("PhononSensitivity::SetOutputFile", "PhonSense003",
                  FatalException, msg);
      output.close();
    } else {
      output << "Run ID,Event ID,Track ID,Particle Name,Start Energy [eV],"
             << "Start X [m],Start Y [m],Start Z [m],Start Time [ns],"
             << "Energy Deposited [eV],Track Weight,End X [m],End Y [m],End Z [m],"
             << "Final Time [ns]\n";
    }
  }
}

G4bool PhononSensitivity::IsHit(const G4Step* step,
                                const G4TouchableHistory*) const {
  /* Phonons tracks are sometimes killed at the boundary in order to spawn new
   * phonon tracks. These tracks that are killed deposit no energy and should
   * not be picked up as hits.
   */
  const G4Track* track = step->GetTrack();
  const G4StepPoint* postStepPoint = step->GetPostStepPoint();
  const G4ParticleDefinition* particle = track->GetDefinition();

  G4bool correctParticle = (particle == G4PhononLong::Definition() ||
                            particle == G4PhononTransFast::Definition() ||
                            particle == G4PhononTransSlow::Definition());

  G4bool correctStatus = (track->GetTrackStatus() == fStopAndKill &&
                          postStepPoint->GetStepStatus() == fGeomBoundary &&
                          step->GetNonIonizingEnergyDeposit() > 0.);

  return correctParticle && correctStatus;
}