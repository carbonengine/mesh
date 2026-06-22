// Copyright © 2025 CCP ehf.

#pragma once

#include <optional>

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> computeFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete()
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

// Device class which holds on to both logical and physical devices and related functionality
class Device
{
public:
	Device();
	~Device();

	VkResult Initialize( VkInstance instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR surface );

	SwapChainSupportDetails GetSwapchainSupportDetails() const;
	QueueFamilyIndices GetQueueFamilyIndices() const;

	VkDevice GetLogicalDevice() const;
	VkPhysicalDevice GetPhysicalDevice() const;

	VkQueue GetGraphicsQueue() const;
	VkQueue GetPresentQueue() const;
	VkQueue GetComputeQueue() const;

	VkPhysicalDeviceMemoryProperties GetMemoryProperties() const;

	uint32_t GetMemoryTypeIndex( uint32_t typeBits, VkMemoryPropertyFlags properties );

private:
	VkDevice m_logicalDevice;
	VkPhysicalDevice m_physicalDevice;
	VkQueue m_graphicsQueue;
	VkQueue m_presentQueue;
	VkQueue m_computeQueue;
	VkPhysicalDeviceMemoryProperties m_memoryProperties;

	QueueFamilyIndices m_familyIndices;
	SwapChainSupportDetails m_swapchainSupport;

	VkResult pickPhysicalDevice( VkInstance instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR surface );
	VkResult createLogicalDevice( const VkAllocationCallbacks* allocator );

	bool IsDeviceSuitable( VkPhysicalDevice device, VkSurfaceKHR surface );
	SwapChainSupportDetails QuerySwapChainSupport( VkPhysicalDevice device, VkSurfaceKHR surface );
	bool DeviceSupportsExtensions( VkPhysicalDevice device );
	QueueFamilyIndices FindQueueFamilies( VkPhysicalDevice device, VkSurfaceKHR surface );
};
