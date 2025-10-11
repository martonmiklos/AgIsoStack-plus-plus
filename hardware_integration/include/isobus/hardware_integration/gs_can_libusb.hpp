//================================================================================================
/// @file gs_can_libusb.cpp
///
/// @brief An userspace (libusb based) driver for gs_usb (Geschwister Schneider USB-CAN) compatible devices
/// @author Miklos Marton
///
/// @copyright 2025 The Open-Agriculture Developers
//================================================================================================
#pragma once

#include <string>

#include "isobus/hardware_integration/can_hardware_plugin.hpp"
#include "isobus/isobus/can_message_frame.hpp"

#if ANDROID
#include <libusb/libusb.h>
#else
#include <libusb-1.0/libusb.h>
#endif
#include <string>

#define GSUSB_VID 0x1d50
#define GSUSB_PID 0x606f

#define CAN_EFF_FLAG 0x80000000U /* EFF/SFF is set in the MSB */
#define CAN_RTR_FLAG 0x40000000U /* remote transmission request */
#define CAN_ERR_FLAG 0x20000000U /* error message frame */

namespace isobus
{
	//================================================================================================
	/// @class GS_CANInterface
	///
	/// @brief A CAN Driver for Linux socket CAN
	//================================================================================================
	class GS_CAN_Interface : public CANHardwarePlugin
	{
	public:
#if defined(ANDROID)
        /// @brief Constructor for the GS USB CAN driver
        explicit GS_CAN_Interface();
#else
        /// @brief Constructor for the GS USB CAN driver
		/// @param[in] serial The device serial number, if blank the first enumerated device will be opened
		explicit GS_CAN_Interface(const std::string serial = "");
#endif
		/// @brief The destructor for GS_CAN_Interface
		virtual ~GS_CAN_Interface();

		/// @brief Returns if the socket connection is valid
		/// @returns `true` if connected, `false` if not connected
		bool get_is_valid() const override;

		/// @brief Closes the socket
		void close() override;

		/// @brief Connects to the socket
		void open() override;

		/// @brief Returns a frame from the hardware (synchronous), or `false` if no frame can be read.
		/// @param[in, out] canFrame The CAN frame that was read
		/// @returns `true` if a CAN frame was read, otherwise `false`
		bool read_frame(isobus::CANMessageFrame &canFrame) override;

		/// @brief Writes a frame to the bus (synchronous)
		/// @param[in] canFrame The frame to write to the bus
		/// @returns `true` if the frame was written, otherwise `false`
		bool write_frame(const isobus::CANMessageFrame &canFrame) override;

#if defined(ANDROID)
        /// @brief Set the file descriptor of the CAN interface to be opened.
        /// It is mandatory to call this function before calling open
        /// @param[in] descriptor file descriptor acquired from JAVA via the UsbDevice.getFileDescriptor()
        void set_file_descriptor(int descriptor);
#else
		/// @brief Set the serial number of the target adapter, which only works if the device is not open
		/// @param[in] serial The serial number of the adapter to be opened. If blank the first enumerated device will be opened
		/// @returns `true` if the target serial was changed, otherwise `false` (if the device is open this will return false)
		bool set_serial(const std::string &serial);
#endif
	private:
		/* Device specific constants */
		enum gs_usb_breq
		{
			GS_USB_BREQ_HOST_FORMAT = 0,
			GS_USB_BREQ_BITTIMING,
			GS_USB_BREQ_MODE,
			GS_USB_BREQ_BERR,
			GS_USB_BREQ_BT_CONST,
			GS_USB_BREQ_DEVICE_CONFIG,
			GS_USB_BREQ_TIMESTAMP,
			GS_USB_BREQ_IDENTIFY,
			GS_USB_BREQ_GET_USER_ID,
			GS_USB_BREQ_QUIRK_CANTACT_PRO_DATA_BITTIMING = GS_USB_BREQ_GET_USER_ID,
			GS_USB_BREQ_SET_USER_ID,
			GS_USB_BREQ_DATA_BITTIMING,
			GS_USB_BREQ_BT_CONST_EXT,
			GS_USB_BREQ_SET_TERMINATION,
			GS_USB_BREQ_GET_TERMINATION,
			GS_USB_BREQ_GET_STATE,
		};

		enum gs_can_mode
		{
			/* reset a channel. turns it off */
			GS_CAN_MODE_RESET = 0,
			/* starts a channel */
			GS_CAN_MODE_START
		};

		enum gs_can_state
		{
			GS_CAN_STATE_ERROR_ACTIVE = 0,
			GS_CAN_STATE_ERROR_WARNING,
			GS_CAN_STATE_ERROR_PASSIVE,
			GS_CAN_STATE_BUS_OFF,
			GS_CAN_STATE_STOPPED,
			GS_CAN_STATE_SLEEPING
		};
#pragma pack(push, 1)
		struct gs_host_frame
		{
			uint32_t echo_id;
			uint32_t can_id;
			uint8_t can_dlc;
			uint8_t channel;
			uint8_t flags;
			uint8_t reserved;
			uint8_t data[8];
		};
#pragma pack(pop)

#pragma pack(push, 1)
		struct gs_host_bittiming
		{
			uint32_t prop_seg;
			uint32_t phase_seg1;
			uint32_t phase_seg2;
			uint32_t sjw;
			uint32_t brp; // Baud rate prescaler
		};
#pragma pack(pop)

#pragma pack(push, 1)
		struct gs_device_mode
		{
			uint32_t mode;
			uint32_t flags;
		} __packed;
#pragma pack(pop)

#pragma pack(push, 1)
		struct gs_device_bt_const
		{
			uint32_t feature;
			uint32_t fclk_can;
			uint32_t tseg1_min;
			uint32_t tseg1_max;
			uint32_t tseg2_min;
			uint32_t tseg2_max;
			uint32_t sjw_max;
			uint32_t brp_min;
			uint32_t brp_max;
			uint32_t brp_inc;
		};
#pragma pack(pop)

		bool start();
		bool readConfig();
		bool reset();
		bool setBaudRate();
		int findEndPoints();
        libusb_context *ctx = nullptr;
		libusb_device_handle *handle = nullptr;
        uint8_t ep_in, ep_out;
#if defined(ANDROID)
        intptr_t file_descriptor = 0;
        bool openedWithoutFd = false;
#else
        std::string target_serial, opened_serial;
#endif
	};
}
