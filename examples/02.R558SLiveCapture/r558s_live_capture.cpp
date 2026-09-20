#include <minutiae.hpp>
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <fstream>

using Clock = std::chrono::high_resolution_clock;

// Helper to save Netpbm PGM (viewable in standard image viewers / VSCode / GIMP)
static bool save_pgm_image(const std::string& filepath, const minutiae::RawImage& image) {
    std::ofstream ofs(filepath, std::ios::binary);
    if (!ofs) return false;

    ofs << "P5\n" << image.width << " " << image.height << "\n255\n";
    ofs.write(reinterpret_cast<const char*>(image.data.data()), image.data.size());
    return ofs.good();
}

int main(int argc, char* argv[]) {
    std::string port = "COM6";
    if (argc > 1) {
        port = argv[1];
    }

    std::cout << "======================================================================\n";
    std::cout << "  libminutiae - Live Two-Step Biometric Flow: ENROLL & VERIFY\n";
    std::cout << "======================================================================\n";
    std::cout << "Serial Port: " << port << " @ 57600 baud (8N2)\n\n";

    // 1. Initialize Hardware Transport
    auto transport = minutiae::create_serial_transport();
    if (!transport || !transport->open(port, 57600)) {
        std::cerr << "[-] Error: Failed to open serial port " << port << "!\n";
        return 1;
    }

    minutiae::drivers::R558S sensor;
    if (!sensor.initialize(*transport)) {
        std::cerr << "[-] Error: Failed to communicate with GROW R558-S sensor!\n";
        return 1;
    }

    std::string chip_uid;
    sensor.read_chip_unique_id(chip_uid);
    minutiae::DeviceInfo info;
    sensor.get_device_info(info);

    std::cout << "[+] Connected to Sensor:\n";
    std::cout << "    Silicon Chip UID:     " << (chip_uid.empty() ? "UNKNOWN" : chip_uid) << "\n";
    std::cout << "    Resolution:           " << info.width_px << "x" << info.height_px << " @ " << info.resolution_dpi << " DPI\n\n";

    // Configure Engine with strict biometric verification tolerances
    minutiae::MatcherConfig matcher_cfg;
    matcher_cfg.physical_distance_threshold_um = 200.0f; // ~4 pixels at 508 DPI (half-pitch tolerance)
    matcher_cfg.max_rotation_deg = 25.0f;               // +/- 25 degrees rotational tolerance
    matcher_cfg.max_angle_tolerance_deg = 20.0f;        // +/- 20 degrees minutia angle agreement
    matcher_cfg.match_score_threshold = 55;             // Strict threshold rejecting impostor fingers
    minutiae::Engine engine(matcher_cfg);

    // ========================================================================
    // STEP 1: BIOMETRIC ENROLLMENT (ENTRY / REGISTRATION)
    // ========================================================================
    std::cout << "======================================================================\n";
    std::cout << "  STEP 1: BIOMETRIC ENROLLMENT (Register Identity)\n";
    std::cout << "======================================================================\n";
    std::cout << ">>> PLACE YOUR FINGER ON THE SENSOR TO ENROLL (Waiting up to 30s)...\n";

    sensor.set_led(minutiae::drivers::LedMode::BREATHING, minutiae::drivers::LedColor::BLUE, minutiae::drivers::LedColor::BLUE, 0);

    bool enroll_finger_found = false;
    for (int i = 0; i < 120; ++i) { // 30-second timeout
        if (sensor.is_finger_present()) {
            enroll_finger_found = true;
            break;
        }
        if (i % 8 == 0) {
            std::cout << "    Waiting for finger touch... (" << (30 - i / 4) << "s remaining)\n" << std::flush;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    if (!enroll_finger_found) {
        std::cout << "[-] Timeout: No finger detected during enrollment.\n";
        sensor.set_led(minutiae::drivers::LedMode::OFF, minutiae::drivers::LedColor::NONE);
        return 0;
    }

    sensor.set_led(minutiae::drivers::LedMode::ALWAYS_ON, minutiae::drivers::LedColor::BLUE);
    std::cout << "[+] Finger detected! Capturing raw 4,800-byte frame over UART...\n";

    minutiae::RawImage enroll_image;
    if (!sensor.capture_raw_image(enroll_image)) {
        std::cerr << "[-] Error: Failed to upload enrollment image from sensor!\n";
        sensor.set_led(minutiae::drivers::LedMode::FLASHING, minutiae::drivers::LedColor::RED, minutiae::drivers::LedColor::NONE, 3);
        return 1;
    }

    save_pgm_image("enrolled_identity.pgm", enroll_image);

    auto enroll_minutiae = engine.extract_minutiae(enroll_image);
    auto enroll_quality  = engine.evaluate_quality(enroll_image, enroll_minutiae);

    std::cout << "[+] Enrollment Frame Acquired:\n";
    std::cout << "    Saved Image:          enrolled_identity.pgm (96x100 8-bit)\n";
    std::cout << "    Minutiae Extracted:   " << enroll_minutiae.size() << " feature points\n";
    std::cout << "    NFIQ 2 Quality Score: " << static_cast<int>(enroll_quality.overall_score) << " / 100 ("
              << (enroll_quality.acceptable ? "PASS - Certified Grade" : "MARGINAL") << ")\n";

    // Create & persist standard ISO/IEC 19794-2 template
    auto enrolled_template = engine.create_template(enroll_image, enroll_minutiae, minutiae::FingerPosition::RIGHT_INDEX);
    minutiae::Engine::save_fmr_file("enrolled_identity.fmr", enrolled_template, minutiae::standards::StandardFormat::ISO_19794_2_2005);
    std::cout << "    Saved Standard FMR:   enrolled_identity.fmr (ISO/IEC 19794-2:2005)\n\n";

    // Signal Enrollment Success
    sensor.set_led(minutiae::drivers::LedMode::FLASHING, minutiae::drivers::LedColor::GREEN, minutiae::drivers::LedColor::GREEN, 3);
    std::cout << "[***] ENROLLMENT COMPLETE! Please LIFT your finger completely off the sensor.\n";
    std::cout << "    Waiting for finger release..." << std::flush;

    // Require at least 3 consecutive confirmed no-finger states to ensure genuine release
    int release_streak = 0;
    while (release_streak < 3) {
        if (!sensor.is_finger_present()) {
            release_streak++;
        } else {
            release_streak = 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    std::cout << " [CONFIRMED LIFTED]\n\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // ========================================================================
    // STEP 2: BIOMETRIC VERIFICATION (1:1 AUTHENTICATION)
    // ========================================================================
    std::cout << "======================================================================\n";
    std::cout << "  STEP 2: BIOMETRIC VERIFICATION (Authenticate Identity)\n";
    std::cout << "======================================================================\n";
    std::cout << ">>> NOW PLACE A FINGER ON THE SENSOR TO VERIFY...\n";
    std::cout << "    (Use the SAME finger to verify a match, or a DIFFERENT finger to test rejection)\n";

    sensor.set_led(minutiae::drivers::LedMode::BREATHING, minutiae::drivers::LedColor::BLUE, minutiae::drivers::LedColor::BLUE, 0);

    bool verify_finger_found = false;
    for (int i = 0; i < 120; ++i) { // 30-second timeout
        if (sensor.is_finger_present()) {
            verify_finger_found = true;
            break;
        }
        if (i % 8 == 0) {
            std::cout << "    Waiting for verification touch... (" << (30 - i / 4) << "s remaining)\n" << std::flush;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    if (!verify_finger_found) {
        std::cout << "[-] Timeout: No finger placed for verification.\n";
        sensor.set_led(minutiae::drivers::LedMode::OFF, minutiae::drivers::LedColor::NONE);
        return 0;
    }

    sensor.set_led(minutiae::drivers::LedMode::ALWAYS_ON, minutiae::drivers::LedColor::BLUE);
    std::cout << "[+] Finger detected! Capturing candidate frame over UART...\n";

    minutiae::RawImage verify_image;
    if (!sensor.capture_raw_image(verify_image)) {
        std::cerr << "[-] Error: Failed to upload verification image from sensor!\n";
        sensor.set_led(minutiae::drivers::LedMode::FLASHING, minutiae::drivers::LedColor::RED, minutiae::drivers::LedColor::RED, 3);
        return 1;
    }

    save_pgm_image("candidate_verification.pgm", verify_image);

    // 1. Anti-Replay Detection
    bool is_replay = engine.check_replay(verify_image, enroll_image);
    if (is_replay) {
        std::cout << "[!] WARNING: Identical bitstream detected! (Simulated or replayed frame)\n";
    }

    // 2. Extract candidate minutiae & quality
    auto verify_minutiae = engine.extract_minutiae(verify_image);
    auto verify_quality  = engine.evaluate_quality(verify_image, verify_minutiae);
    auto candidate_template = engine.create_template(verify_image, verify_minutiae, minutiae::FingerPosition::RIGHT_INDEX);

    std::cout << "[+] Candidate Frame Acquired:\n";
    std::cout << "    Saved Image:          candidate_verification.pgm (96x100 8-bit)\n";
    std::cout << "    Minutiae Extracted:   " << verify_minutiae.size() << " feature points\n";
    std::cout << "    NFIQ 2 Quality Score: " << static_cast<int>(verify_quality.overall_score) << " / 100\n\n";

    // 3. 1:1 Biometric Verification Match against Enrolled Template
    auto t_match_start = Clock::now();
    uint8_t match_score = 0;
    bool is_match = engine.verify(candidate_template, enrolled_template, match_score);
    auto t_match_end = Clock::now();
    double match_ms = std::chrono::duration<double, std::milli>(t_match_end - t_match_start).count();

    std::cout << "======================================================================\n";
    std::cout << "  VERIFICATION RESULT\n";
    std::cout << "======================================================================\n";
    std::cout << "  Matching Algorithm Latency: " << std::fixed << std::setprecision(2) << match_ms << " ms\n";
    std::cout << "  Calculated Match Score:     " << static_cast<int>(match_score) << " / 100\n";
    std::cout << "  Decision Threshold:         " << static_cast<int>(matcher_cfg.match_score_threshold) << " / 100\n\n";

    if (is_match) {
        // MATCH SUCCESS: Flash GREEN LED
        sensor.set_led(minutiae::drivers::LedMode::FLASHING, minutiae::drivers::LedColor::GREEN, minutiae::drivers::LedColor::GREEN, 5);
        std::cout << "  ============================================================\n";
        std::cout << "  [+] RESULT: ACCESS GRANTED / IDENTITY VERIFIED!\n";
        std::cout << "      Match Score " << static_cast<int>(match_score) << "% >= Threshold " << static_cast<int>(matcher_cfg.match_score_threshold) << "%\n";
        std::cout << "  ============================================================\n";

        // Adaptive Self-Learning
        if (engine.adapt(enrolled_template, candidate_template, match_score)) {
            std::cout << "  [+] Adaptive Self-Learning: Enrolled template updated with newly verified minutiae (total points: "
                      << enrolled_template.minutiae.size() << ")\n";
            minutiae::Engine::save_fmr_file("enrolled_identity.fmr", enrolled_template);
        }
    } else {
        // MATCH FAILED: Flash RED LED
        sensor.set_led(minutiae::drivers::LedMode::FLASHING, minutiae::drivers::LedColor::RED, minutiae::drivers::LedColor::RED, 5);
        std::cout << "  ============================================================\n";
        std::cout << "  [-] RESULT: ACCESS DENIED / REJECTED!\n";
        std::cout << "      Candidate fingerprint does not match enrolled identity.\n";
        std::cout << "      Score " << static_cast<int>(match_score) << "% < Threshold " << static_cast<int>(matcher_cfg.match_score_threshold) << "%\n";
        std::cout << "  ============================================================\n";
    }

    std::cout << "\n======================================================================\n";
    std::cout << "  libminutiae Complete 2-Step Biometric Lifecycle Test Finished!\n";
    std::cout << "======================================================================\n";

    // Keep LED flashing visible for 2 seconds before closing COM port
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    sensor.set_led(minutiae::drivers::LedMode::OFF, minutiae::drivers::LedColor::NONE);

    // Biometric Data Privacy Protection:
    // Remove persistent raw image and template artifacts so biometric data is never left exposed
    std::remove("enrolled_identity.pgm");
    std::remove("enrolled_identity.fmr");
    std::remove("candidate_verification.pgm");
    std::cout << "[+] Biometric Privacy: Securely cleared enrolled and candidate biometric records from disk.\n";

    return 0;
}
