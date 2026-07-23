// analysis.c
// 用法：root -l -q analysis.c
// 输出：phonon_analysis.root 及四张 png 图片

#include <TFile.h>
#include <TTree.h>
#include <TCanvas.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TStyle.h>
#include <TLatex.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

void analysis() {
    gStyle->SetOptStat(0);

    TFile *outFile = new TFile("phonon_analysis.root", "RECREATE");

    int runID, eventID, trackID;
    char particleName[20];
    double startEnergy, startX, startY, startZ, startTime;
    double energyDeposited;
    double trackWeight;
    double endX, endY, endZ, finalTime;

    TTree *tree = new TTree("phononTree", "Phonon hits from G4CMP");
    tree->Branch("runID", &runID, "runID/I");
    tree->Branch("eventID", &eventID, "eventID/I");
    tree->Branch("trackID", &trackID, "trackID/I");
    tree->Branch("particleName", particleName, "particleName/C");
    tree->Branch("startEnergy", &startEnergy, "startEnergy/D");
    tree->Branch("startX", &startX, "startX/D");
    tree->Branch("startY", &startY, "startY/D");
    tree->Branch("startZ", &startZ, "startZ/D");
    tree->Branch("startTime", &startTime, "startTime/D");
    tree->Branch("energyDeposited", &energyDeposited, "energyDeposited/D");
    tree->Branch("trackWeight", &trackWeight, "trackWeight/D");
    tree->Branch("endX", &endX, "endX/D");
    tree->Branch("endY", &endY, "endY/D");
    tree->Branch("endZ", &endZ, "endZ/D");
    tree->Branch("finalTime", &finalTime, "finalTime/D");

    std::ifstream infile("phonon_hits.txt");
    if (!infile.is_open()) {
        std::cerr << "Error: cannot open phonon_hits.txt" << std::endl;
        return;
    }

    std::string line;
    std::getline(infile, line);   // 跳过标题行
    std::cout << "Header: " << line << std::endl;

    int totalLines = 0, goodLines = 0, badLines = 0;

    while (std::getline(infile, line)) {
        totalLines++;
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> tokens;
        while (std::getline(ss, token, ',')) {
            tokens.push_back(token);
        }

        if (tokens.size() < 15) {
            std::cerr << "Warning: skipping line " << totalLines
                      << " (only " << tokens.size() << " fields): " << line << std::endl;
            badLines++;
            continue;
        }

        try {
            runID        = std::stoi(tokens[0]);
            eventID      = std::stoi(tokens[1]);
            trackID      = std::stoi(tokens[2]);
            strncpy(particleName, tokens[3].c_str(), 19);
            particleName[19] = '\0';

            startEnergy     = std::stod(tokens[4]);
            startX          = std::stod(tokens[5]);
            startY          = std::stod(tokens[6]);
            startZ          = std::stod(tokens[7]);
            startTime       = std::stod(tokens[8]);
            energyDeposited = std::stod(tokens[9]);
            trackWeight     = std::stod(tokens[10]);
            endX            = std::stod(tokens[11]);
            endY            = std::stod(tokens[12]);
            endZ            = std::stod(tokens[13]);
            finalTime       = std::stod(tokens[14]);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing line " << totalLines << ": " << line
                      << "\n" << e.what() << std::endl;
            badLines++;
            continue;
        }

        tree->Fill();
        goodLines++;
        if (goodLines % 100000 == 0) {
            std::cout << "Processed " << goodLines << " good events..." << std::endl;
        }
    }

    infile.close();
    std::cout << "Finished reading: " << goodLines << " good lines, "
              << badLines << " bad lines, " << totalLines << " total lines.\n";

    tree->Write();

    // ---------- 定义直方图 ----------
    // 击中位置分布 (计数)
    TH2F *h_xy = new TH2F("h_xy", "Hit Counts (XY);End X [mm];End Y [mm]",
                          200, -5, 5, 200, -2, 2);

    // 能量沉积谱 (范围改为 0 - 0.005 eV)
    TH1F *h_edep = new TH1F("h_edep", "Energy Deposited;Energy (eV);Counts",
                            100, 0, 0.005);

    // 总沉积能量 vs 传播时间 (时间范围改为 0 - 100 ns)
    TH1F *h_edep_dt = new TH1F("h_edep_dt",
                               "Total Energy Deposited vs Propagation Time;Time (ns);Total Energy (eV)",
                               200, 0, 100);

    // 能量沉积空间分布 (以能量沉积为权重)
    TH2F *h_edep_xy = new TH2F("h_edep_xy",
                               "Total Energy Deposited (XY);End X [mm];End Y [mm]",
                               200, -5, 5, 200, -2, 2);

    // ---------- 填充直方图 ----------
    tree->Draw("endY*1000:endX*1000>>h_xy", "energyDeposited>0", "COLZ");
    h_xy->SetTitle("Hit Counts (XY);End X [mm];End Y [mm]");

    tree->Draw("energyDeposited>>h_edep", "energyDeposited>0", "");
    h_edep->SetTitle("Energy Deposited;Energy (eV);Counts");

    tree->Draw("finalTime-startTime>>h_edep_dt",
               "energyDeposited>0 && (finalTime-startTime)>0",
               "");
    h_edep_dt->SetTitle("Total Energy Deposited vs Propagation Time;Time (ns);Total Energy (eV)");

    tree->Draw("endY*1000:endX*1000>>h_edep_xy", "energyDeposited", "COLZ");
    h_edep_xy->SetTitle("Total Energy Deposited (XY);End X [mm];End Y [mm]");

    // ---------- 绘制并保存图片 ----------
    // 计算总击中数和总能量沉积
    double totalHits = h_xy->GetEntries();
    double totalEnergy = 0.;
    for (int bx = 1; bx <= h_edep_xy->GetNbinsX(); ++bx) {
        for (int by = 1; by <= h_edep_xy->GetNbinsY(); ++by) {
            totalEnergy += h_edep_xy->GetBinContent(bx, by);
        }
    }

    TCanvas *c1 = new TCanvas("c1", "Hit Counts", 800, 600);
    h_xy->Draw("COLZ");
    TLatex latex;
    latex.SetNDC();
    latex.SetTextSize(0.035);
    latex.SetTextColor(kBlack);
    latex.DrawLatex(0.65, 0.85, Form("Total hits: %.0f", totalHits));
    c1->SaveAs("absorption_position_xy.png");

    TCanvas *c2 = new TCanvas("c2", "Energy Spectrum", 800, 600);
    h_edep->Draw();
    c2->SaveAs("energy_deposited.png");

    TCanvas *c3 = new TCanvas("c3", "Total Energy vs Time", 800, 600);
    h_edep_dt->Draw("HIST");
    c3->SaveAs("total_energy_vs_time.png");

    TCanvas *c4 = new TCanvas("c4", "Energy XY", 800, 600);
    h_edep_xy->Draw("COLZ");
    latex.DrawLatex(0.60, 0.85, Form("Total Energy: %.4f eV", totalEnergy));
    c4->SaveAs("energy_deposited_xy.png");

    // 将直方图写入 root 文件
    h_xy->Write();
    h_edep->Write();
    h_edep_dt->Write();
    h_edep_xy->Write();

    outFile->Close();
    std::cout << "Analysis complete. Output saved to phonon_analysis.root and PNG files.\n";
}