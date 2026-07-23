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
#include "G4CMPUtils.hh"
#include <fstream>
#include <iostream>

PhononSensitivity::PhononSensitivity(G4String name) :
  G4CMPElectrodeSensitivity(name), fileName(""), lowEFileName("lowE_phonons.txt") {
  SetOutputFile(PhononConfigManager::GetHitOutput());
  // 打开低能声子文件（追加模式）
  lowEOutput.open(lowEFileName, std::ios_base::app);
  if (lowEOutput.good()) {
    lowEOutput << "Run ID,Event ID,Track ID,Start Energy [eV]" << std::endl;
  } else {
    G4cerr << "Warning: Could not open lowE_phonons.txt for writing." << G4endl;
  }
}

PhononSensitivity::~PhononSensitivity() {
  if (output.is_open()) output.close();
  if (!output.good()) {
    G4cerr << "Error closing output file, " << fileName << ".\n"
           << "Expect bad things like loss of data.";
  }
  if (lowEOutput.is_open()) lowEOutput.close();
}

G4bool PhononSensitivity::ProcessHits(G4Step* step, G4TouchableHistory* history) {
  G4Track* track = step->GetTrack();
  const G4ParticleDefinition* particle = track->GetDefinition();

  // 仅对声子检查能量
  if (G4CMP::IsPhonon(particle)) {
    G4double energy = track->GetKineticEnergy();
    if (energy < lowEThreshold) {
      // 记录低能声子信息
      if (lowEOutput.is_open() && lowEOutput.good()) {
        G4RunManager* runMan = G4RunManager::GetRunManager();
        lowEOutput << runMan->GetCurrentRun()->GetRunID() << ','
                   << runMan->GetCurrentEvent()->GetEventID() << ','
                   << track->GetTrackID() << ','
                   << energy / CLHEP::eV << std::endl;
      }
      // 杀死声子，不沉积能量（也可将能量设为沉积，视需求而定）
      track->SetTrackStatus(fStopAndKill);
      // 避免生成电极击中
      return false;
    }
  }

  // 正常处理：调用基类（内部会调用 IsHit 并填充电极击中）
  return G4CMPElectrodeSensitivity::ProcessHits(step, history);
}

void PhononSensitivity::EndOfEvent(G4HCofThisEvent* HCE) {
  G4int HCID = G4SDManager::GetSDMpointer()->GetCollectionID(hitsCollection);
  auto* hitCol = static_cast<G4CMPElectrodeHitsCollection*>(HCE->GetHC(HCID));
  std::vector<G4CMPElectrodeHit*>* hitVec = hitCol->GetVector();

  G4RunManager* runMan = G4RunManager::GetRunManager();

  // 有效 Resonator 区域 (单位：mm)
  const double xLeftMin  = -3.775, xLeftMax  = -3.325;
  const double xRightMin =  3.325, xRightMax =  3.775;
  const double yMin = -0.52, yMax = 0.077665;

  if (output.good()) {
    for (G4CMPElectrodeHit* hit : *hitVec) {
      double x = hit->GetFinalPosition().x();   // mm
      double y = hit->GetFinalPosition().y();

      bool inLeft  = (x >= xLeftMin  && x <= xLeftMax  && y >= yMin && y <= yMax);
      bool inRight = (x >= xRightMin && x <= xRightMax && y >= yMin && y <= yMax);
      if (!inLeft && !inRight) continue;

      output << runMan->GetCurrentRun()->GetRunID() << ','
             << runMan->GetCurrentEvent()->GetEventID() << ','
             << hit->GetTrackID() << ','
             << hit->GetParticleName() << ','
             << hit->GetStartEnergy()/CLHEP::eV << ','
             << hit->GetStartPosition().getX()/CLHEP::m << ','
             << hit->GetStartPosition().getY()/CLHEP::m << ','
             << hit->GetStartPosition().getZ()/CLHEP::m << ','
             << hit->GetStartTime()/CLHEP::ns << ','
             << hit->GetEnergyDeposit()/CLHEP::eV << ','
             << hit->GetWeight() << ','
             << hit->GetFinalPosition().getX()/CLHEP::m << ','
             << hit->GetFinalPosition().getY()/CLHEP::m << ','
             << hit->GetFinalPosition().getZ()/CLHEP::m << ','
             << hit->GetFinalTime()/CLHEP::ns << '\n';
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