// analysis2.c
// 用法：root -l -q analysis2.c
// 输出：energy_deposited_xy.png 和 energy_pie.png

#include <TFile.h>
#include <TCanvas.h>
#include <TH2F.h>
#include <TPie.h>
#include <TLatex.h>
#include <TStyle.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

void analysis2() {
    gStyle->SetOptStat(0);

    // ========== 读取有效电极击中数据 (phonon_hits.txt) ==========
    std::ifstream infile("phonon_hits.txt");
    if (!infile.is_open()) {
        std::cerr << "Error: cannot open phonon_hits.txt" << std::endl;
        return;
    }

    std::string line;
    std::getline(infile, line);   // 跳过标题行
    std::cout << "phonon_hits header: " << line << std::endl;

    double endX, endY, eDep;
    double totalDepEnergy = 0.0;
    int goodHits = 0;

    std::vector<double> v_endX, v_endY, v_eDep;

    while (std::getline(infile, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> tokens;
        while (std::getline(ss, token, ',')) {
            tokens.push_back(token);
        }
        if (tokens.size() < 15) continue;
        try {
            endX = std::stod(tokens[11]);
            endY = std::stod(tokens[12]);
            eDep = std::stod(tokens[9]);
            if (eDep <= 0) continue;
            v_endX.push_back(endX);
            v_endY.push_back(endY);
            v_eDep.push_back(eDep);
            totalDepEnergy += eDep;
            goodHits++;
        } catch (...) {
            continue;
        }
    }
    infile.close();
    std::cout << "Effective hits: " << goodHits << ", total deposited energy: "
              << totalDepEnergy << " eV" << std::endl;

    // ========== 读取低能声子数据 (lowE_phonons.txt) ==========
    std::ifstream lowFile("lowE_phonons.txt");
    double totalLowEnergy = 0.0;
    int lowCount = 0;
    if (lowFile.is_open()) {
        std::getline(lowFile, line);   // header
        while (std::getline(lowFile, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string token;
            std::vector<std::string> tokens;
            while (std::getline(ss, token, ',')) {
                tokens.push_back(token);
            }
            if (tokens.size() < 4) continue;
            try {
                double e = std::stod(tokens[3]);
                totalLowEnergy += e;
                lowCount++;
            } catch (...) { continue; }
        }
        lowFile.close();
        std::cout << "Low‑energy phonons: " << lowCount
                  << ", total energy: " << totalLowEnergy << " eV" << std::endl;
    } else {
        std::cerr << "Warning: cannot open lowE_phonons.txt, set low energy = 0" << std::endl;
    }

    // ========== 记录的总能量 ==========
    double totalRecordedEnergy = totalDepEnergy + totalLowEnergy;

    // ========== 能量沉积 XY 直方图 ==========
    TH2F *h_edep_xy = new TH2F("h_edep_xy",
                               "Total Energy Deposited (XY);End X [mm];End Y [mm]",
                               200, -5, 5, 200, -2, 2);
    for (size_t i = 0; i < v_endX.size(); ++i) {
        h_edep_xy->Fill(v_endX[i] * 1000., v_endY[i] * 1000., v_eDep[i]);
    }

    TCanvas *c1 = new TCanvas("c1", "Energy Deposited XY", 800, 600);
    h_edep_xy->Draw("COLZ");
    TLatex latex;
    latex.SetNDC();
    latex.SetTextSize(0.035);
    latex.SetTextColor(kBlack);
    latex.DrawLatex(0.55, 0.85, Form("Total Dep. Energy: %.4f eV", totalDepEnergy));
    c1->SaveAs("energy_deposited_xy.png");

  // ========== 扇形图 ==========
    TCanvas *c2 = new TCanvas("c2", "Energy Partition", 800, 600);

    // 创建临时直方图，用于 TPie
    TH1F *h_pie = new TH1F("h_pie", "Energy Partition", 2, 0, 2);
    h_pie->SetBinContent(1, totalDepEnergy);
    h_pie->SetBinContent(2, totalLowEnergy);
    h_pie->GetXaxis()->SetBinLabel(1, Form("Deposited (%.2f eV)", totalDepEnergy));
    h_pie->GetXaxis()->SetBinLabel(2, Form("Sub-gap Loss (%.2f eV)", totalLowEnergy));

    TPie *pie = new TPie(h_pie);
    pie->Draw();

    // 添加总记录能量注释
    TLatex latex2;
    latex2.SetNDC();
    latex2.SetTextSize(0.035);
    latex2.SetTextColor(kBlack);
    latex2.DrawLatex(0.15, 0.85, Form("Total recorded: %.4f eV", totalRecordedEnergy));

    c2->SaveAs("energy_pie.png");

    std::cout << "Analysis complete. Output: energy_deposited_xy.png, energy_pie.png" << std::endl;
}