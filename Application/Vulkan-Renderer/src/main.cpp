#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <thread>
#include <vector>
#include <sstream>
#include <iomanip>
#include <sstream>
#include <string>

#include "VulkanRenderer.hpp"

#include "simpleble/SimpleBLE.h"

std::vector<SimpleBLE::Peripheral> peripherals;
std::vector<std::pair<SimpleBLE::BluetoothUUID, SimpleBLE::BluetoothUUID>> uuids;
int rx = 0, ry = 0, rz = 0;
std::string commingStr = "";
int seletedPeripheral = -1, seletedUUID = -1;

GLFWwindow* window;
VulkanRenderer vulkanRenderer;

std::string hexToString(SimpleBLE::ByteArray bytes) {
        std::string a, b;
        std::stringstream ss;
        
        for (auto byte : bytes) {
            ss << std::hex << std::setfill('0') << std::setw(2) << (uint32_t)((uint8_t)byte) << " ";
            ss >> a;
            b = b + a;
        }
        return b;
}

void initWindow(std::string wName = "Vulkan Application", const int width = 800, const int height = 600)
{
	// Initialise GLFW
	glfwInit();

	// Set GLFW to NOT work with OpenGL
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	// glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	
	window = glfwCreateWindow(width, height, wName.c_str(), nullptr, nullptr);
	
	glfwSetFramebufferSizeCallback(window, vulkanRenderer.framebufferResizeCallback);
}


void initBle()
{
	auto adapter_list = SimpleBLE::Adapter::get_adapters();

    if (adapter_list.size() == 0) {
        std::cout << "No adapter was found." << std::endl;
    }

    std::cout << "Available adapters: \n";
    int i = 0;
    for (auto& adapter : adapter_list) {
        std::cout << "[" << i++ << "] " << adapter.identifier() << " [" << adapter.address() << "]" << std::endl;
    }

    int adapter_selection = -1;
    while(adapter_selection < 0 || adapter_selection > adapter_list.size() - 1) {
        std::cout << "Please select an adapter: ";
        std::cin >> adapter_selection;
    }

    SimpleBLE::Adapter& adapter = adapter_list[adapter_selection];

    adapter.set_callback_on_scan_start([]() { std::cout << "Scan started." << std::endl; });

    adapter.set_callback_on_scan_stop([]() { std::cout << "Scan stopped." << std::endl; });

    adapter.set_callback_on_scan_found([&](SimpleBLE::Peripheral peripheral) {
        std::cout << "Found device: " << peripheral.identifier() << " [" << peripheral.address() << "]" << std::endl;
        peripherals.push_back(peripheral);
    });

    // Scan for 5 seconds and return.
    adapter.scan_for(5000);

    std::cout << "The following devices were found:" << std::endl;
    for (size_t i = 0; i < peripherals.size(); i++) {
        std::cout << "[" << i << "] " << peripherals[i].identifier() << " [" << peripherals[i].address() << "]"
                  << std::endl;
    }

    int selection = -1;
    std::cout << "Please select a device to connect to: ";
    std::cin >> selection;

    if (selection >= 0 && selection < peripherals.size()) {
		seletedPeripheral = selection;
        auto peripheral = peripherals[selection];
        std::cout << "Connecting to " << peripheral.identifier() << " [" << peripheral.address() << "]" << std::endl;
        peripheral.connect();

        std::cout << "Successfully connected, printing services and characteristics.." << std::endl;

        // Store all service and characteristic uuids in a vector.
        for (auto service : peripheral.services()) {
            for (auto characteristic : service.characteristics) {
                uuids.push_back(std::make_pair(service.uuid, characteristic));
            }
        }

        std::cout << "The following services and characteristics were found:" << std::endl;
        for (size_t i = 0; i < uuids.size(); i++) {
            std::cout << "[" << i << "] " << uuids[i].first << " " << uuids[i].second << std::endl;
        }

        std::cout << "Please select a characteristic to read: ";
        std::cin >> selection;
        seletedUUID = selection;
	}
}

void processStr(std::string str)
{
    std::string str_rx, str_ry, str_rz;
    str_rx = str.substr(0, str.find('|'));
    str_ry = str.substr(str.find('|')+1, str.size());
    str_rz = str_ry.substr(str_ry.find('|')+1, str_ry.size());
    str_ry = str_ry.substr(0, str_ry.find('|'));
    
    
    rx = std::stoi(str_rx.c_str(), nullptr);
    ry = std::stoi(str_ry.c_str(), nullptr);
    rz = std::stoi(str_rz.c_str(), nullptr);
    commingStr = "";
}

void startGetMessage()
{
    peripherals[seletedPeripheral].notify(
        uuids[seletedUUID].first, uuids[seletedUUID].second, [&](SimpleBLE::ByteArray bytes) {
            // std::cout << "Converted: ";
            for (int i = 0; i < hexToString(bytes).length(); i += 2) {
                std::string byte = hexToString(bytes).substr(i, 2);
                char chr = (char)(int)strtol(byte.c_str(), nullptr, 16);
                commingStr.push_back(chr);
                // std::cout << chr;
            }
            // std::cout << "\r\n" << commingStr << std::endl;
            processStr(commingStr);
            // std::cout << "rx: " << rx << ", ry: " << ry +180 << ", rz: " << rz +180 << std::endl;
            vulkanRenderer.setAngle(rx, ry, rz);
            
        });
        // Sleep(10);
}

int main()
{
    initBle();

	// Create Window
	initWindow("Vulkan Application", 800, 600);

	// Create Vulkan Renderer instance
	if (vulkanRenderer.init(window) == EXIT_FAILURE)
	{
		return EXIT_FAILURE;
	}

	// Loop until closed
	while (!glfwWindowShouldClose(window))
	{
	    startGetMessage();
		glfwPollEvents();
		vulkanRenderer.drawFrame();
	}

	vulkanRenderer.deviceWaitIdle();

	vulkanRenderer.cleanup();

    peripherals[seletedPeripheral].unsubscribe(uuids[seletedUUID].first, uuids[seletedUUID].second);
    peripherals[seletedPeripheral].disconnect();

	// Destroy GLFW window and stop GLFW
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}