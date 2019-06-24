// danteTest.cpp
// Simple test program for XGL Dante

#include <iostream>
#include <stdio.h>
#include <inttypes.h>
#include <DLL_DPP_Callback.h>

uint32_t config_call_id;
uint32_t firm_call_id;

void callback_function (uint16_t type, uint32_t call_id, uint32_t length, uint32_t* data) 
{
    if (call_id == firm_call_id) { 
        if (type == 1 && length == 3) { // As should be for getFirmware call. 
            uint16_t Major = data[0];
            uint16_t Minor = data[1];
            uint16_t Build = data[2];
            std::cout << "Firmware (MASTER): "<<Major<<"."<<Minor<<"."<<Build;
        }
    }
    else if (call_id == config_call_id) {
        if (type == 2 && data[0] == 1) { // As should be for successful configure call. 
            std::cout << "DPP configured." << std::endl;
        }
    }
}


int main(int argc, char *argv[])
{
    std::cout << "Initialization..." << std::endl;
    if (!InitLibrary()) { 
        std::cout << "ERROR: Library not initialized." << std::endl;
    }

    if (add_to_query("192.168.1.120")) {
        std::cout << "Added 192.168.1.120 to the query." << std::endl;
    }


    uint16_t devices = 0;
    get_dev_number(devices); 
    std::cout << "Found devices: " << devices << std::endl;
    
    if (devices < 1) {
        std::cout << "No devices found, exiting: " << std::endl;
        CloseLibrary();
        return -1;
    }

    char* identifier(new char[16]);
    uint16_t slaves = 0;
    uint16_t id_size = 16;
    for (uint16_t i = 1; i <= devices; i++) { 
        get_ids(identifier,i,id_size);
        std::cout << "Device: "<< i << " - Identifier: " << identifier << std::endl;
        get_boards_in_chain(identifier, slaves);
        std::cout << "Slaves: " << (slaves - 1) << std::endl; 
    }

    firm_call_id = getFirmware(identifier, 0); // Firmware asked to master board.

    configuration config;
    config.fast_filter_thr = 80;
    config.max_risetime = 30;
    config.baseline_samples = 64;
    config.gain = 0.43;
    config.peaking_time = 25; 
    config.flat_top = 5;
    config.edge_peaking_time = 1;
    config.edge_flat_top = 1;
    config.reset_recovery_time = 300;

    config_call_id = configure(identifier, 0, config);

    double time = 0.1; // secs.
    uint32_t start_call_id = start(identifier, time, 4096);

    bool running = true; 
    while (running) { 
        // Query MASTER board.
        uint32_t run_call_id = isRunning_system(identifier, 0);
        // Get the answer from the callback or the queue and update the running variable. 
        // If answer not ready do something else... 
        // Otherwise. 
        if (running) { 
            std::cout << "Chain with MASTER serial" <<identifier << " is running...";
            std::cout << std::endl;
        } 
    } std::cout << "Acquisition ended." << std::endl;

    uint64_t data[4096];
    uint32_t spectra_size = 4096;
    uint32_t id;
    statistics stats; // The statistics structure is declared in the header file.

    for (uint16_t brd = 0; brd < slaves + 1; brd++) { 
        getData(identifier, brd, &data[0], id, stats, spectra_size);
        // Plot/store the data somewhere... }
    }
    CloseLibrary();
    return 0;   
}
