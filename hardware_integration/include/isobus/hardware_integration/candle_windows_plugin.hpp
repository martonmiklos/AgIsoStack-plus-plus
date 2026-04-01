//================================================================================================
/// @file candle_windows_plugin.hpp
///
/// @brief An interface for using gs_usb compatible adapters via candle_dll.
/// @attention Use of this plugin is subject to the candle_dll license (LGPL-3.0).
/// @author Open-Agriculture
///
/// @copyright 2026 The Open-Agriculture Developers
//================================================================================================
#ifndef CANDLE_WINDOWS_PLUGIN_HPP
#define CANDLE_WINDOWS_PLUGIN_HPP

#include <cstdint>
#include <string>

#include "isobus/hardware_integration/can_hardware_plugin.hpp"
#include "isobus/isobus/can_hardware_abstraction.hpp"
#include "isobus/isobus/can_message_frame.hpp"

namespace isobus
{
	/// @brief A CAN driver for Windows using candle_dll.
	class CandleWindowsPlugin : public CANHardwarePlugin
	{
	public:
		/// @brief Constructor for the candle_dll CAN driver.
		/// @param[in] channel The candle channel index.
		/// @param[in] bitrate The CAN bitrate in bits/s.
		/// @param[in] deviceIndex The candle device index from the scanned list.
		/// @param[in] dllName The DLL filename/path (for example: "candle.dll").
		explicit CandleWindowsPlugin(std::uint8_t channel = 0,
		                            std::uint32_t bitrate = 250000,
		                            std::uint8_t deviceIndex = 0,
		                            std::string dllName = "candle.dll");

		/// @brief Destructor.
		virtual ~CandleWindowsPlugin();

		/// @brief Returns if the connection with the hardware is valid.
		/// @returns `true` if connected, `false` if not connected.
		bool get_is_valid() const override;

		/// @brief Closes the connection to the hardware.
		void close() override;

		/// @brief Connects to the hardware.
		void open() override;

		/// @brief Reads one frame synchronously.
		/// @param[in, out] canFrame The CAN frame that was read.
		/// @returns `true` if a CAN frame was read.
		bool read_frame(isobus::CANMessageFrame &canFrame) override;

		/// @brief Writes one frame synchronously.
		/// @param[in] canFrame The CAN frame to write.
		/// @returns `true` if the frame was written.
		bool write_frame(const isobus::CANMessageFrame &canFrame) override;

	private:
		struct DriverAPI;
		struct DriverContext;

		bool load_dll();
		void unload_dll();
		bool scan_and_open_device();

		std::uint8_t channel;
		std::uint32_t bitrate;
		std::uint8_t deviceIndex;
		std::string dllName;
		DriverContext *context = nullptr;
	};
}

#endif // CANDLE_WINDOWS_PLUGIN_HPP
