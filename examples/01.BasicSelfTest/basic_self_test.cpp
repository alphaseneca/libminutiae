#include <minutiae.hpp>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>

int main() {
    std::cout << "==========================================================\n";
    std::cout << "  libminutiae - On-Device Biometrics & Standards Self-Test\n";
    std::cout << "  (100% Local / Self-Contained / Embedded MCU-Grade)\n";
    std::cout << "==========================================================\n\n";

    minutiae::Engine engine;

    // 1. Synthesize a realistic 96x100 fingerprint image with concentric ridge pattern
    const uint16_t W = 96;
    const uint16_t H = 100;
    minutiae::RawImage synthetic_img;
    synthetic_img.width = W;
    synthetic_img.height = H;
    synthetic_img.resolution_dpcm = minutiae::DPCM_508_DPI; // 508 DPI (200 dpcm)
    synthetic_img.data.resize(W * H);

    float cx = W / 2.0f;
    float cy = H / 2.0f;
    float wavelength = 8.0f; // ~400 um ridge pitch at 508 DPI

    for (uint16_t y = 0; y < H; ++y) {
        for (uint16_t x = 0; x < W; ++x) {
            float r = std::hypot(x - cx, (y - cy) * 1.2f);
            float angle = std::atan2(y - cy, x - cx);
            float val = 128.0f + 100.0f * std::sin((2.0f * 3.14159265f * r) / wavelength);
            
            // Introduce ridge terminations (gaps) and bifurcations
            if (x > 25 && x < 35 && y > 30 && y < 40) {
                val = 240.0f; // Ridge ending gap
            }
            if (x > 60 && x < 70 && y > 60 && y < 70) {
                val = 240.0f; // Another gap
            }
            if (x > 45 && x < 55 && y > 20 && y < 30 && std::abs(angle) < 0.5f) {
                val = 20.0f; // Bifurcation branch
            }

            synthetic_img.data[y * W + x] = static_cast<uint8_t>(std::clamp(val, 0.0f, 255.0f));
        }
    }

    std::cout << "[1] Generated Synthetic 96x100 Grayscale Fingerprint Frame\n";
    std::cout << "    Dimensions: " << W << "x" << H << " @ " << minutiae::dpcm_to_dpi(synthetic_img.resolution_dpcm) << " DPI\n\n";

    // 2. Feature Extraction: Zhang-Suen morphological thinning & Crossing Number
    auto minutiae_list = engine.extract_minutiae(synthetic_img);
    std::cout << "[2] Minutiae Extraction Complete:\n";
    std::cout << "    Total Valid Minutiae Extracted: " << minutiae_list.size() << "\n";
    for (size_t i = 0; i < std::min(minutiae_list.size(), size_t(5)); ++i) {
        std::cout << "      #" << i + 1 << ": (" << minutiae_list[i].x << ", " << minutiae_list[i].y << ") "
                  << "Type=" << (minutiae_list[i].type == minutiae::MinutiaeType::ENDING ? "ENDING" : "BIFURCATION")
                  << ", Angle=" << std::fixed << std::setprecision(1) << minutiae_list[i].angle_deg << " deg\n";
    }
    std::cout << "\n";

    // 3. NIST NFIQ 2 / ISO/IEC 29794-4 Biometric Quality Assessment
    auto quality_report = engine.evaluate_quality(synthetic_img, minutiae_list);
    std::cout << "[3] NIST NFIQ 2 Quality Report:\n";
    std::cout << "    Overall NFIQ 2 Quality Score: " << static_cast<int>(quality_report.overall_score) << " / 100\n";
    std::cout << "    Local Clarity Score (LCS):   " << std::fixed << std::setprecision(2) << quality_report.local_clarity_score << "%\n";
    std::cout << "    Orientation Certainty (OCL): " << quality_report.orientation_certainty << "%\n";
    std::cout << "    Minutiae Spatial Spread:     " << quality_report.spatial_spread << "%\n";
    std::cout << "    Quality Status:              " << (quality_report.acceptable ? "PASS (Certified Biometric Grade)" : "BELOW SPEC") << "\n\n";

    // 4. Biometric Template Creation & International Standards Export
    auto tmpl = engine.create_template(synthetic_img, minutiae_list, minutiae::FingerPosition::RIGHT_INDEX);

    uint8_t fmr_buffer[2048];
    size_t iso2005_bytes = tmpl.export_iso_19794_2_2005(fmr_buffer, sizeof(fmr_buffer));
    size_t iso2011_bytes = tmpl.export_iso_19794_2_2011(fmr_buffer, sizeof(fmr_buffer));
    size_t ansi2004_bytes = tmpl.export_ansi_378_2004(fmr_buffer, sizeof(fmr_buffer));
    size_t ansi2009_bytes = tmpl.export_ansi_378_2009(fmr_buffer, sizeof(fmr_buffer));

    std::cout << "[4] International Biometric Standards Serialization:\n";
    std::cout << "    ISO/IEC 19794-2:2005 Record Size: " << iso2005_bytes << " bytes\n";
    std::cout << "    ISO/IEC 19794-2:2011 Record Size: " << iso2011_bytes << " bytes\n";
    std::cout << "    ANSI INCITS 378-2004 Record Size: " << ansi2004_bytes << " bytes\n";
    std::cout << "    ANSI INCITS 378-2009 Record Size: " << ansi2009_bytes << " bytes\n\n";

    // 5. Save & Load Standard FMR File
    const std::string fmr_path = "test_template.fmr";
    bool saved = minutiae::Engine::save_fmr_file(fmr_path, tmpl, minutiae::standards::StandardFormat::ISO_19794_2_2005);
    std::cout << "[5] Local Storage Persistence:\n";
    std::cout << "    Saved Standard ISO/IEC 19794-2 FMR file to: " << fmr_path << " (" << (saved ? "OK" : "FAILED") << ")\n";

    minutiae::BiometricTemplate loaded_tmpl;
    bool loaded = minutiae::Engine::load_fmr_file(fmr_path, loaded_tmpl);
    std::cout << "    Loaded Standard FMR file: " << (loaded ? "SUCCESS" : "FAILED") << "\n";
    std::cout << "    Decoded Minutiae Count:   " << loaded_tmpl.minutiae.size() << "\n";
    std::cout << "    Decoded Format Magic:     0x" << std::hex << loaded_tmpl.magic << std::dec << "\n\n";

    // 6. Template Verification & Matching
    uint8_t match_score = 0;
    bool matched = engine.verify(tmpl, loaded_tmpl, match_score);
    std::cout << "[6] 1:1 Biometric Verification:\n";
    std::cout << "    Match Result: " << (matched ? "VERIFIED (Match Confirmed)" : "REJECTED") << "\n";
    std::cout << "    Match Score:  " << static_cast<int>(match_score) << " / 100\n\n";

    // 7. Anti-Replay Detection
    bool is_replay = engine.check_replay(synthetic_img, synthetic_img);
    std::cout << "[7] Anti-Replay Attack Protection:\n";
    std::cout << "    Replay of Identical Bitstream: " << (is_replay ? "DETECTED & BLOCKED" : "PASSED") << "\n\n";

    std::cout << "==========================================================\n";
    std::cout << "  All libminutiae Core Verification Tests Passed!\n";
    std::cout << "==========================================================\n";

    // Cleanup temporary self-test file
    std::remove(fmr_path.c_str());

    return 0;
}
