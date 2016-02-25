/* By Matìj Mainuš <matej.mainus@gmail.com>
 * Demo app that causes an unknown error code -1000013000. 
 * Error occured in vkCreateSwapchainKHR() if "VK_LAYER_GOOGLE_unique_objects" is enabled
 *
 *
 * Just creates VkInstance, gets first VkPhysicalDevice, VkDevice and graphics queue
 * then in crash() creates VkSurfaceKHR from window and gets preferred properties from 
 * surface that use in VkSwapchainCreateInfoKHR. Then calls vkCreateSwapchainKHR where
 * asserts on error -1000013000
 * 
 * If you remove "VK_LAYER_GOOGLE_unique_objects" from layers vector in 
 * createVkInstance() then vkCreateSwapchainKHR returns VkResult::VK_SUCCESS
 */

#include <windows.h>
#include <fcntl.h>
#include <io.h>

#include <string>
#include <iostream>
#include <cassert>
#include <array>
#include <vector>

#include <vulkan/vulkan.h>

#define VK_VERIFY(expr)\
	assert(expr == VkResult::VK_SUCCESS)

#define VK_GET_INSTANCE_PROC_ADDR(inst, entrypoint) \
	fp##entrypoint = (PFN_vk##entrypoint) vkGetInstanceProcAddr(inst, "vk"#entrypoint); \
    assert(fp##entrypoint != NULL)      

#define VK_GET_DEVICE_PROC_ADDR(dev, entrypoint) \
    fp##entrypoint = (PFN_vk##entrypoint) vkGetDeviceProcAddr(dev, "vk"#entrypoint); \
    assert(fp##entrypoint != NULL) 

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}

class Main
{
public:

	Main(HINSTANCE hInstance) :
		mHinstance(hInstance)
	{
		createWindow();
		createVKInstance();
		createVkDevice();
	}

	void createWindow()
	{
		WNDCLASSEX wndClass;

		wndClass.cbSize = sizeof(WNDCLASSEX);
		wndClass.style = CS_HREDRAW | CS_VREDRAW;
		wndClass.lpfnWndProc = WndProc;
		wndClass.cbClsExtra = 0;
		wndClass.cbWndExtra = 0;
		wndClass.hInstance = mHinstance;
		wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
		wndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
		wndClass.lpszMenuName = NULL;
		wndClass.lpszClassName = L"VkUnknownError";
		wndClass.hIconSm = LoadIcon(NULL, IDI_WINLOGO);

		RegisterClassEx(&wndClass);	
		
		int screenWidth = GetSystemMetrics(SM_CXSCREEN);
		int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	
		RECT windowRect;
		windowRect.left = (long)screenWidth / 2 - mWinWidth / 2;
		windowRect.right = (long) mWinWidth;
		windowRect.top = (long)screenHeight / 2 - mWinHeight / 2;
		windowRect.bottom = (long) mWinHeight;

		AdjustWindowRectEx(&windowRect,
			WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 
			FALSE,
			WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);

		mHWND = CreateWindowEx(0,
			L"VkUnknownError",
			L"VkUnknownError",
			WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPSIBLINGS ,
			windowRect.left,
			windowRect.top,
			windowRect.right,
			windowRect.bottom,
			NULL,
			NULL,
			mHinstance,
			NULL);

		ShowWindow(mHWND, SW_SHOW);
		SetForegroundWindow(mHWND);
		SetFocus(mHWND);
	}

	uint32_t findQueueIndex(VkPhysicalDevice phyDevice, VkQueueFlags flags)
	{
		uint32_t queueCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(phyDevice, &queueCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueProps(queueCount);
		vkGetPhysicalDeviceQueueFamilyProperties(phyDevice, &queueCount, queueProps.data());

		std::vector<VkQueueFamilyProperties>::iterator it = queueProps.begin();

		for (uint32_t queueIndex = 0; it != queueProps.end(); ++it, ++queueIndex)
		{
			if ((queueProps[queueIndex].queueFlags & flags) == flags)
				return queueIndex;
		}

		throw std::runtime_error("No graphics qeue found for selected GPU");
	}

	void createVKInstance()
	{
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "";
		appInfo.pEngineName = "";
		appInfo.apiVersion = VK_MAKE_VERSION(1, 0, 3);

		std::vector<const char*> extensions;
		std::vector<const char*> layers;

		extensions.insert(extensions.end(),
		{
			VK_KHR_SURFACE_EXTENSION_NAME, 
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
			VK_EXT_DEBUG_REPORT_EXTENSION_NAME
		});

		layers.insert(layers.end(),
		{
			"VK_LAYER_GOOGLE_unique_objects"
		});

		VkInstanceCreateInfo instanceCreateInfo = {};
		instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceCreateInfo.pNext = NULL;
		instanceCreateInfo.pApplicationInfo = &appInfo;

		instanceCreateInfo.enabledExtensionCount = (uint32_t) extensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = extensions.data();
		instanceCreateInfo.enabledLayerCount = (uint32_t)layers.size();
		instanceCreateInfo.ppEnabledLayerNames = layers.data();

		VK_VERIFY(vkCreateInstance(&instanceCreateInfo, nullptr, &mVkInstance));
	}

	void createVkDevice()
	{
		uint32_t devicesCount = 0;
		VK_VERIFY(vkEnumeratePhysicalDevices(mVkInstance, &devicesCount, nullptr));

		assert(devicesCount > 0);

		std::vector<VkPhysicalDevice> devices(devicesCount);
		VK_VERIFY(vkEnumeratePhysicalDevices(mVkInstance, &devicesCount, devices.data()));

		std::vector<const char*> extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		std::array<float, 1> queuePriorities = { 0.0f };

		mVkPhysDevice = devices[0];

		uint32_t graphicsQueueIndex = findQueueIndex(mVkPhysDevice, VK_QUEUE_GRAPHICS_BIT);
		
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.pNext = nullptr;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.queueFamilyIndex = graphicsQueueIndex;
		queueCreateInfo.pQueuePriorities = queuePriorities.data();

		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.pNext = nullptr;
		deviceCreateInfo.queueCreateInfoCount = 1;
		deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
		deviceCreateInfo.enabledExtensionCount = (uint32_t)extensions.size();
		deviceCreateInfo.ppEnabledExtensionNames = extensions.data();

		VK_VERIFY(vkCreateDevice(mVkPhysDevice, &deviceCreateInfo, nullptr, &mVkDevice));

		vkGetDeviceQueue(mVkDevice, graphicsQueueIndex, 0, &mGrapicsQueue);
	}

	void crash()
	{
		PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR;
		PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR;
		PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR;
		PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR;
		PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR;
		PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR;

		VK_GET_INSTANCE_PROC_ADDR(mVkInstance, GetPhysicalDeviceSurfaceSupportKHR);
		VK_GET_INSTANCE_PROC_ADDR(mVkInstance, GetPhysicalDeviceSurfaceCapabilitiesKHR);
		VK_GET_INSTANCE_PROC_ADDR(mVkInstance, GetPhysicalDeviceSurfaceFormatsKHR);
		VK_GET_INSTANCE_PROC_ADDR(mVkInstance, GetPhysicalDeviceSurfacePresentModesKHR);
		VK_GET_DEVICE_PROC_ADDR(mVkDevice, CreateSwapchainKHR);
		VK_GET_DEVICE_PROC_ADDR(mVkDevice, GetSwapchainImagesKHR);

		VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
		surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceCreateInfo.pNext = nullptr;
		surfaceCreateInfo.hinstance = mHinstance;
		surfaceCreateInfo.hwnd = mHWND;

		VK_VERIFY(vkCreateWin32SurfaceKHR(mVkInstance, &surfaceCreateInfo, nullptr, &mVkSurface));

		VkSurfaceCapabilitiesKHR surfCaps;
		VK_VERIFY(fpGetPhysicalDeviceSurfaceCapabilitiesKHR(mVkPhysDevice, mVkSurface, &surfCaps));

		uint32_t presentModeCount;
		VK_VERIFY(fpGetPhysicalDeviceSurfacePresentModesKHR(mVkPhysDevice, mVkSurface, &presentModeCount, nullptr));

		std::vector<VkPresentModeKHR> presentModes(presentModeCount);
		VK_VERIFY(fpGetPhysicalDeviceSurfacePresentModesKHR(mVkPhysDevice, mVkSurface, &presentModeCount, presentModes.data()));

		uint32_t formatCount;
		VK_VERIFY(fpGetPhysicalDeviceSurfaceFormatsKHR(mVkPhysDevice, mVkSurface, &formatCount, nullptr));

		std::vector<VkSurfaceFormatKHR> surfFormats(formatCount);
		VK_VERIFY(fpGetPhysicalDeviceSurfaceFormatsKHR(mVkPhysDevice, mVkSurface, &formatCount, surfFormats.data()));

		VkSwapchainCreateInfoKHR swapchainCI = {};
		swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchainCI.pNext = nullptr;
		swapchainCI.imageExtent = surfCaps.currentExtent;
		swapchainCI.surface = mVkSurface;
		swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		swapchainCI.imageArrayLayers = 1;
		swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCI.queueFamilyIndexCount = 0;
		swapchainCI.pQueueFamilyIndices = nullptr;
		swapchainCI.oldSwapchain = VK_NULL_HANDLE;
		swapchainCI.clipped = true;
		swapchainCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchainCI.presentMode = VK_PRESENT_MODE_FIFO_KHR;

		for (size_t i = 0; i < presentModeCount; i++)
		{
			if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				swapchainCI.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}

			if ((swapchainCI.presentMode != VK_PRESENT_MODE_MAILBOX_KHR) && (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR))
			{
				swapchainCI.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			}
		}

		swapchainCI.minImageCount = surfCaps.minImageCount + 1;
		if ((surfCaps.maxImageCount > 0) && (swapchainCI.minImageCount > surfCaps.maxImageCount))
		{
			swapchainCI.minImageCount = surfCaps.maxImageCount;
		}

		if (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
		{
			swapchainCI.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		}
		else {
			swapchainCI.preTransform = surfCaps.currentTransform;
		}

		assert(formatCount > 0);

		if (formatCount == 1)
		{
			swapchainCI.imageFormat = surfFormats[0].format == VK_FORMAT_UNDEFINED ? VK_FORMAT_R8G8B8_UNORM : surfFormats[0].format;
			swapchainCI.imageColorSpace = surfFormats[0].colorSpace;
		}
		else
		{
			swapchainCI.imageFormat = surfFormats[0].format;
			swapchainCI.imageColorSpace = surfFormats[0].colorSpace;
		}

		VkSwapchainKHR swapChain;
		VK_VERIFY(fpCreateSwapchainKHR(mVkDevice, &swapchainCI, nullptr, &swapChain));

	}

private:
	HINSTANCE mHinstance;
	HWND mHWND;

	VkInstance mVkInstance;
	VkPhysicalDevice mVkPhysDevice;
	VkDevice mVkDevice;
	VkQueue mGrapicsQueue;

	VkSurfaceKHR mVkSurface;

	const unsigned int mWinWidth = 800;
	const unsigned int mWinHeight = 600;
};

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
	Main main(hInstance);

	main.crash();

	return 0;
}