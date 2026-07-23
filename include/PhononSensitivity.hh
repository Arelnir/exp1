/***********************************************************************\
 * This software is licensed under the terms of the GNU General Public *
 * License version 3 or later. See G4CMP/LICENSE for the full license. *
\***********************************************************************/

#ifndef PhononSensitivity_h
#define PhononSensitivity_h 1

#include "G4CMPElectrodeSensitivity.hh"
#include <fstream>
#include <string>

class PhononSensitivity final : public G4CMPElectrodeSensitivity {
public:
  PhononSensitivity(G4String name);
  virtual ~PhononSensitivity();

  // No copies
  PhononSensitivity(const PhononSensitivity&) = delete;
  PhononSensitivity& operator=(const PhononSensitivity&) = delete;
  PhononSensitivity(PhononSensitivity&&) = delete;
  PhononSensitivity& operator=(PhononSensitivity&&) = delete;

  virtual void EndOfEvent(G4HCofThisEvent*);
  void SetOutputFile(const G4String& fn);

protected:
  virtual G4bool IsHit(const G4Step*, const G4TouchableHistory*) const;
  // 重写 ProcessHits，添加低能声子检查
  virtual G4bool ProcessHits(G4Step*, G4TouchableHistory*) override;

private:
  // 主输出文件（电极击中记录）
  std::ofstream output;
  G4String fileName;

  // 低能声子记录文件
  std::ofstream lowEOutput;
  G4String lowEFileName;

  // 低能声子能量阈值（与超导能隙 2Δ 一致）
  static constexpr G4double lowEThreshold = 2 * 173.715e-6 * CLHEP::eV; // 2Δ for Al
};

#endif