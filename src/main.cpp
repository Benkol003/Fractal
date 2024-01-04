#include "vulkan/vulkan.h"
#include "GLFW/glfw3.h"


#include <string>
#include <iostream>
#include <vector>
#include <algorithm>

#include <cstring>

void keyCallback(GLFWwindow* window,int key,int scancode,int action, int mods) { //idk what a scancode is 

    if(action==GLFW_PRESS){
        switch(key){ //i dont have the time to learn howto implement custom operators, and certainly not for making it for a enum, which isnt even a class type

            case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window,true);break;
        }
    }

}

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation",
};

const std::vector<const char*> extensions = {
    "VK_KHR_swapchain"
};

bool checkValidationLayerSupport() {

    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (auto& layerName : validationLayers) {
        if (std::find_if(availableLayers.begin(), availableLayers.end(), 
            [layerName](VkLayerProperties layer) {return strcmp(layer.layerName, layerName) == 0; }) == availableLayers.end()) return false;
    }
    return true;
}

int main(){
try{

    if (!glfwInit()) return -1;

    if(!glfwVulkanSupported()) throw std::runtime_error("Vulkan is not supported.\n");
    
    //window setup
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE,false);

    GLFWwindow* root=glfwCreateWindow(800,600,"Vulkan Application",nullptr,nullptr);
    if (root==nullptr) {
        std::cerr<<"Failed to create glfw window.\n";
        glfwTerminate();
        return -1;
    }
    glfwSetInputMode(root,GLFW_STICKY_KEYS,true); //#RM? TODO what does this do
    glfwSetKeyCallback(root,keyCallback);

    //vulkan setup

    //vulkan validation layers
    //NDEBUG is in c/C++ standards; _DEBUG is for microsoft/MSVC only.

    #ifdef NDEBUG
        const bool enableValidationLayers = false;
        std::cout << "Vulkan validation disabled.\n";
    #else
        const bool enableValidationLayers = true;
        std::cout << "Vulkan validation enabled.\n";
    #endif

    if (enableValidationLayers && !checkValidationLayerSupport()) throw std::runtime_error("Vulkan validation layers are not available.\n");

    VkInstance instance;
    uint32_t glfwExtensionCount=0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    VkInstanceCreateInfo instanceCreateInfo{};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; //all vulkan structs require you to specify  the sType
    instanceCreateInfo.enabledExtensionCount = glfwExtensionCount;
    instanceCreateInfo.ppEnabledExtensionNames = glfwExtensions;
    
    //enable/disable validation layers
    if (enableValidationLayers) {
        instanceCreateInfo.enabledLayerCount = validationLayers.size();
        instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else instanceCreateInfo.enabledLayerCount = 0;

    //by default vulkan validation layers print to std out
    //you can change this - see https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Validation_layers#page_Message-callback

    if(vkCreateInstance(&instanceCreateInfo,nullptr,&instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vulkan instance.");
        //TODO need to be able to test on a system where this fails and get the error codes
        // https://vulkan-tutorial.com/en/Drawing_a_triangle/Setup/Instance
    }

    //vulkan window surface
    VkSurfaceKHR surface;
    if(glfwCreateWindowSurface(instance, root, nullptr, &surface) != VK_SUCCESS) 
        throw std::runtime_error("failed to create window surface.\n");

    

    //acquiring a vulkan device
    VkPhysicalDevice device = VK_NULL_HANDLE; //bruh yet another name for null;
    uint32_t deviceCount=0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) throw std::runtime_error("No available vulkan devices.\n");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    VkPhysicalDeviceProperties deviceProperties{};
    VkPhysicalDeviceFeatures deviceFeatures{};
    for (auto& d : devices) {
        
        vkGetPhysicalDeviceProperties(d, &deviceProperties);
        //query here
        if (deviceProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) continue;

        
        vkGetPhysicalDeviceFeatures(d, &deviceFeatures);
        //query here

        //if not continue:
        //select device
        device = d;
        break; //such that device properties and features are for current device
    }

    if (device == VK_NULL_HANDLE) throw std::runtime_error("Failed to find a suitable vulkan device from those available.\n");
    std::cout << "Vulkan device: " << deviceProperties.deviceName << '\n';

    //select queue family
    //TODO: better checks for queue compatibility and allow choosing a different device if it has a more suitable queue family?
    uint32_t queueFamilyCount=0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0) throw std::runtime_error("Device has no available queue families.\n");
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
    std::cout<<"Queue families: "<<queueFamilyCount<<'\n';
    int queueFamilyIndex = -1;
    VkBool32 supported = false;
    for (int i = 0; i < queueFamilyCount; ++i) {
        VkBool32 support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device,i,surface,&support);
        if(!support) {
            supported=true; continue;
        }
        if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) continue; //can also have e.g VK_QUEUE_COMPUTE_BIT for compute
        if (queueFamilies[i].queueCount < 1) {
            continue;
        }
        //check https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkQueueFlagBits.html
        else {
            queueFamilyIndex = i;
        }
    }
    if(supported==false) throw std::runtime_error("Failed to find queue family to support surface KHR.\n");
    if (queueFamilyIndex == -1) throw std::runtime_error("Failed to find suitable queue family from the device.\n");
    std::cout << "Queue Family Index: " << queueFamilyIndex << '\n';

    //setup logical device
    VkDevice logicalDevice;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.queueCreateInfoCount = 1;

    //needed device features: TODO
    deviceFeatures = {};
    deviceCreateInfo.pEnabledFeatures=&deviceFeatures;
    
    if(vkCreateDevice(device,&deviceCreateInfo,nullptr,&logicalDevice)!= VK_SUCCESS) 
        throw std::runtime_error("Failed to crate logical device.\n");


    VkQueue graphicsQueue;
    vkGetDeviceQueue(logicalDevice,queueFamilyIndex,0,&graphicsQueue);

    VkQueue presentQueue;
    


    //TODO: a lot of these functions, and C api calls in particular, we do two calls, 1. to get array size, 2. then to fill array of that size.
    //can this be compacted?
    //or can we replace this by following pNext?

    //main window loop
    while(!glfwWindowShouldClose(root)){

        glfwPollEvents();
     }

    //cleanup
    vkDestroySurfaceKHR(instance,surface,nullptr);
    vkDestroyDevice(logicalDevice,nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(root);
    glfwTerminate();
    return 0;

}catch(std::exception &e){
    std::cout<<"Unhandled exception in main: "<<e.what();
} 
}