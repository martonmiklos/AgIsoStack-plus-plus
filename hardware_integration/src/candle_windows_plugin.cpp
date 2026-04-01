//================================================================================================
/// @file candle_windows_plugin.cpp
///
/// @brief An interface for using gs_usb compatible adapters via candle_dll.
/// @attention Use of this plugin is subject to the candle_dll license (LGPL-3.0).
/// @author Open-Agriculture
///
/// @copyright 2026 The Open-Agriculture Developers
//================================================================================================

#include "isobus/hardware_integration/candle_windows_plugin.hpp"
#include "isobus/isobus/can_stack_logger.hpp"
#include "isobus/utility/to_string.hpp"

#include <windows.h>
#include <cstring>
#include <thread>

namespace isobus
{
	using candle_list_handle = void *;
	using candle_handle = void *;

	enum candle_frametype_t
	{
		CANDLE_FRAMETYPE_UNKNOWN,
		CANDLE_FRAMETYPE_RECEIVE,
		CANDLE_FRAMETYPE_ECHO,
		CANDLE_FRAMETYPE_ERROR,
		CANDLE_FRAMETYPE_TIMESTAMP_OVFL
	};

	enum candle_device_mode_flags_t
	{
		CANDLE_MODE_NORMAL = 0x00
	};

	enum candle_err_t
	{
		CANDLE_ERR_OK = 0
	};

#pragma pack(push, 1)
	struct candle_frame_t
	{
		std::uint32_t echo_id;
		std::uint32_t can_id;
		std::uint8_t can_dlc;
		std::uint8_t channel;
		std::uint8_t flags;
		std::uint8_t reserved;
		std::uint8_t data[8];
		std::uint32_t timestamp_us;
	};
#pragma pack(pop)

	struct CandleWindowsPlugin::DriverAPI
	{
		bool(__stdcall *list_scan)(candle_list_handle *list) = nullptr;
		bool(__stdcall *list_free)(candle_list_handle list) = nullptr;
		bool(__stdcall *dev_get)(candle_list_handle list, std::uint8_t dev_num, candle_handle *hdev) = nullptr;
		bool(__stdcall *dev_open)(candle_handle hdev) = nullptr;
		bool(__stdcall *dev_close)(candle_handle hdev) = nullptr;
		bool(__stdcall *dev_free)(candle_handle hdev) = nullptr;
		bool(__stdcall *channel_set_bitrate)(candle_handle hdev, std::uint8_t ch, std::uint32_t bitrate) = nullptr;
		bool(__stdcall *channel_start)(candle_handle hdev, std::uint8_t ch, candle_device_mode_flags_t flags) = nullptr;
		bool(__stdcall *channel_stop)(candle_handle hdev, std::uint8_t ch) = nullptr;
		bool(__stdcall *frame_send)(candle_handle hdev, std::uint8_t ch, candle_frame_t *frame, bool wait_send, std::uint32_t timeout_ms) = nullptr;
		bool(__stdcall *frame_read)(candle_handle hdev, candle_frame_t *frame, std::uint32_t timeout_ms) = nullptr;
		candle_frametype_t(__stdcall *frame_type)(candle_frame_t *frame) = nullptr;
		candle_err_t(__stdcall *dev_last_error)(candle_handle hdev) = nullptr;
		const char *(__stdcall *error_text)(candle_err_t errnum) = nullptr;
	};

	struct CandleWindowsPlugin::DriverContext
	{
		HMODULE moduleHandle = nullptr;
		DriverAPI api;
		candle_list_handle listHandle = nullptr;
		candle_handle deviceHandle = nullptr;
		bool isOpen = false;
	};

	namespace
	{
		constexpr std::uint32_t CAN_EFF_FLAG = 0x80000000;
		constexpr std::uint32_t CAN_RTR_FLAG = 0x40000000;
		constexpr std::uint32_t CAN_EFF_MASK = 0x1FFFFFFF;
		constexpr std::uint32_t CAN_SFF_MASK = 0x000007FF;

		template<typename T>
		T get_symbol(HMODULE moduleHandle, const char *symbolName)
		{
			return reinterpret_cast<T>(GetProcAddress(moduleHandle, symbolName));
		}
	}

	CandleWindowsPlugin::CandleWindowsPlugin(std::uint8_t channel,
	                                         std::uint32_t bitrate,
	                                         std::uint8_t deviceIndex,
	                                         std::string dllName) :
	  channel(channel),
	  bitrate(bitrate),
	  deviceIndex(deviceIndex),
	  dllName(std::move(dllName)),
	  context(new DriverContext())
	{
	}

	CandleWindowsPlugin::~CandleWindowsPlugin()
	{
		close();
	}

	bool CandleWindowsPlugin::get_is_valid() const
	{
		return (nullptr != context) && context->isOpen;
	}

	void CandleWindowsPlugin::close()
	{
		if (nullptr == context)
		{
			return;
		}

		if (context->isOpen)
		{
			context->api.channel_stop(context->deviceHandle, channel);
			context->api.dev_close(context->deviceHandle);
			context->isOpen = false;
		}
		if (nullptr != context->deviceHandle)
		{
			context->api.dev_free(context->deviceHandle);
			context->deviceHandle = nullptr;
		}
		if (nullptr != context->listHandle)
		{
			context->api.list_free(context->listHandle);
			context->listHandle = nullptr;
		}
		unload_dll();
	}

	void CandleWindowsPlugin::open()
	{
		if (!load_dll())
		{
			return;
		}

		if (!scan_and_open_device())
		{
			close();
			return;
		}

		if (!context->api.channel_set_bitrate(context->deviceHandle, channel, bitrate))
		{
			CANStackLogger::error("[Candle] Failed to set bitrate to " + isobus::to_string(bitrate));
			close();
			return;
		}

		if (!context->api.channel_start(context->deviceHandle, channel, CANDLE_MODE_NORMAL))
		{
			CANStackLogger::error("[Candle] Failed to start channel " + isobus::to_string(channel));
			close();
			return;
		}

		context->isOpen = true;
	}

	bool CandleWindowsPlugin::read_frame(isobus::CANMessageFrame &canFrame)
	{
		if (!get_is_valid())
		{
			return false;
		}

		candle_frame_t frame = { 0 };
		if (!context->api.frame_read(context->deviceHandle, &frame, 1))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			return false;
		}

		candle_frametype_t frameType = context->api.frame_type(&frame);
		if (CANDLE_FRAMETYPE_RECEIVE != frameType)
		{
			return false;
		}

		canFrame.dataLength = frame.can_dlc;
		canFrame.isExtendedFrame = (0 != (frame.can_id & CAN_EFF_FLAG));
		canFrame.identifier = canFrame.isExtendedFrame ? (frame.can_id & CAN_EFF_MASK) : (frame.can_id & CAN_SFF_MASK);
		canFrame.isRemoteFrame = (0 != (frame.can_id & CAN_RTR_FLAG));
		canFrame.timestamp_us = frame.timestamp_us;
		std::memcpy(canFrame.data, frame.data, canFrame.dataLength);
		return true;
	}

	bool CandleWindowsPlugin::write_frame(const isobus::CANMessageFrame &canFrame)
	{
		if (!get_is_valid())
		{
			return false;
		}

		candle_frame_t frame = { 0 };
		frame.can_dlc = canFrame.dataLength;
		frame.can_id = canFrame.identifier;
		if (canFrame.isExtendedFrame)
		{
			frame.can_id |= CAN_EFF_FLAG;
		}
		if (canFrame.isRemoteFrame)
		{
			frame.can_id |= CAN_RTR_FLAG;
		}
		std::memcpy(frame.data, canFrame.data, canFrame.dataLength);

		return context->api.frame_send(context->deviceHandle, channel, &frame, true, 10);
	}

	bool CandleWindowsPlugin::load_dll()
	{
		if (nullptr == context)
		{
			return false;
		}
		if (nullptr != context->moduleHandle)
		{
			return true;
		}

		context->moduleHandle = LoadLibraryA(dllName.c_str());
		if (nullptr == context->moduleHandle)
		{
			CANStackLogger::error("[Candle] Failed to load DLL: " + dllName);
			return false;
		}

		context->api.list_scan = get_symbol<decltype(context->api.list_scan)>(context->moduleHandle, "candle_list_scan");
		context->api.list_free = get_symbol<decltype(context->api.list_free)>(context->moduleHandle, "candle_list_free");
		context->api.dev_get = get_symbol<decltype(context->api.dev_get)>(context->moduleHandle, "candle_dev_get");
		context->api.dev_open = get_symbol<decltype(context->api.dev_open)>(context->moduleHandle, "candle_dev_open");
		context->api.dev_close = get_symbol<decltype(context->api.dev_close)>(context->moduleHandle, "candle_dev_close");
		context->api.dev_free = get_symbol<decltype(context->api.dev_free)>(context->moduleHandle, "candle_dev_free");
		context->api.channel_set_bitrate = get_symbol<decltype(context->api.channel_set_bitrate)>(context->moduleHandle, "candle_channel_set_bitrate");
		context->api.channel_start = get_symbol<decltype(context->api.channel_start)>(context->moduleHandle, "candle_channel_start");
		context->api.channel_stop = get_symbol<decltype(context->api.channel_stop)>(context->moduleHandle, "candle_channel_stop");
		context->api.frame_send = get_symbol<decltype(context->api.frame_send)>(context->moduleHandle, "candle_frame_send");
		context->api.frame_read = get_symbol<decltype(context->api.frame_read)>(context->moduleHandle, "candle_frame_read");
		context->api.frame_type = get_symbol<decltype(context->api.frame_type)>(context->moduleHandle, "candle_frame_type");
		context->api.dev_last_error = get_symbol<decltype(context->api.dev_last_error)>(context->moduleHandle, "candle_dev_last_error");
		context->api.error_text = get_symbol<decltype(context->api.error_text)>(context->moduleHandle, "candle_error_text");

		if ((nullptr == context->api.list_scan) ||
		    (nullptr == context->api.list_free) ||
		    (nullptr == context->api.dev_get) ||
		    (nullptr == context->api.dev_open) ||
		    (nullptr == context->api.dev_close) ||
		    (nullptr == context->api.dev_free) ||
		    (nullptr == context->api.channel_set_bitrate) ||
		    (nullptr == context->api.channel_start) ||
		    (nullptr == context->api.channel_stop) ||
		    (nullptr == context->api.frame_send) ||
		    (nullptr == context->api.frame_read) ||
		    (nullptr == context->api.frame_type) ||
		    (nullptr == context->api.dev_last_error) ||
		    (nullptr == context->api.error_text))
		{
			CANStackLogger::error("[Candle] Missing symbol(s) in DLL: " + dllName);
			unload_dll();
			return false;
		}
		return true;
	}

	void CandleWindowsPlugin::unload_dll()
	{
		if ((nullptr != context) && (nullptr != context->moduleHandle))
		{
			FreeLibrary(context->moduleHandle);
			context->moduleHandle = nullptr;
		}
	}

	bool CandleWindowsPlugin::scan_and_open_device()
	{
		if (!context->api.list_scan(&context->listHandle))
		{
			CANStackLogger::error("[Candle] Failed to scan for devices.");
			return false;
		}
		if (!context->api.dev_get(context->listHandle, deviceIndex, &context->deviceHandle))
		{
			CANStackLogger::error("[Candle] Failed to get device index " + isobus::to_string(deviceIndex));
			return false;
		}
		if (!context->api.dev_open(context->deviceHandle))
		{
			candle_err_t err = context->api.dev_last_error(context->deviceHandle);
			std::string errorText = "unknown";
			const char *dllErrorText = context->api.error_text(err);
			if (nullptr != dllErrorText)
			{
				errorText = dllErrorText;
			}
			CANStackLogger::error("[Candle] Failed to open device. Error: " + errorText);
			return false;
		}
		return true;
	}
}
