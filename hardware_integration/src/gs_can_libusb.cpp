//================================================================================================
/// @file gs_can_libusb.cpp
///
/// @brief An userspace (libusb based) driver for gs_usb compatible devices
/// @author Miklos Marton
///
/// @copyright 2025 The Open-Agriculture Developers
//================================================================================================
#include "isobus/hardware_integration/gs_can_libusb.hpp"
#include <cstring>
#include "isobus/isobus/can_stack_logger.hpp"

#if defined(__ANDROID__)
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define htole32(x) (x)
#define le32toh(x) (x)
#else
#define htole32(x) __builtin_bswap32(x)
        #define le32toh(x) __builtin_bswap32(x)
#endif
#else
#include <endian.h>  // vagy portable változat
#endif

namespace isobus
{
#if defined(ANDROID)
    GS_CAN_Interface::GS_CAN_Interface()
    {
        libusb_set_option(nullptr, LIBUSB_OPTION_LOG_LEVEL, 4);
        libusb_set_option(ctx, LIBUSB_OPTION_NO_DEVICE_DISCOVERY, NULL);
        if (libusb_init(&ctx) < 0)
        {
            LOG_CRITICAL("Failed to initialize libusb");
        }
    }
#else
	GS_CAN_Interface::GS_CAN_Interface(const std::string serial) :
	  target_serial(serial)
	{
        if (libusb_init(&ctx) < 0)
        {
            LOG_CRITICAL("Failed to initialize libusb");
        }
	}
#endif

	GS_CAN_Interface::~GS_CAN_Interface()
	{
		close();
	}

	bool GS_CAN_Interface::get_is_valid() const
	{
		return handle != nullptr;
	}

	void GS_CAN_Interface::close()
	{
		if (handle)
		{
			libusb_detach_kernel_driver(handle, 0);
			libusb_close(handle);
			handle = nullptr;
		}
        openedWithoutFd = false;
	}

	void GS_CAN_Interface::open()
	{
#if defined(ANDROID)
        if (file_descriptor == 0) {
            openedWithoutFd = true;
        } else {
            libusb_wrap_sys_device(NULL, (intptr_t) file_descriptor, &handle);
        }
#else
		libusb_device **devs = nullptr;
		ssize_t cnt;

		cnt = libusb_get_device_list(ctx, &devs);
		if (cnt < 0)
		{
			LOG_ERROR("Error getting USB device list");
			libusb_exit(ctx);
			return;
		}

		for (ssize_t i = 0; i < cnt; ++i)
		{
			libusb_device *dev = devs[i];
			libusb_device_descriptor desc;

			if (libusb_get_device_descriptor(dev, &desc) < 0)
			{
				LOG_WARNING("Failed to get device descriptor");
				continue;
			}

			if (desc.idVendor == GSUSB_VID && desc.idProduct == GSUSB_PID)
			{
				libusb_device_handle *temp_handle;
				if (libusb_open(dev, &temp_handle) != 0)
				{
					LOG_WARNING("Failed to open device matching VID/PID");
					continue;
				}

				if (desc.iSerialNumber)
				{
					unsigned char serial[256];
					int r = libusb_get_string_descriptor_ascii(temp_handle, desc.iSerialNumber, serial, sizeof(serial));
					if (r > 0)
					{
						std::string serial_str(reinterpret_cast<char *>(serial));
						LOG_DEBUG("Found device with serial: " + serial_str);
						if (target_serial.empty())
						{
							opened_serial = serial_str;
							handle = temp_handle;
							break;
						}
						else
						{
							if (serial_str == target_serial)
							{
								opened_serial = target_serial;
								handle = temp_handle;
								LOG_INFO("Matched gs_usb device with serial: " + target_serial);
								break;
							}
						}
					}
					else
					{
						LOG_WARNING("Failed to get serial number string");
					}
				}
				libusb_close(temp_handle);
			}
		}

		libusb_free_device_list(devs, 1);
#endif
		if (!handle)
		{
#ifdef ANDROID
            LOG_ERROR("Unable to open gs_usb device");
#else
			LOG_ERROR("No matching gs_usb device found with serial: " + target_serial);
#endif
			libusb_exit(ctx);
		}
		else
		{
			int r = 0;
			if (libusb_kernel_driver_active(handle, 0))
			{
				r = libusb_detach_kernel_driver(handle, 0);
				if (r != 0)
				{
					LOG_ERROR("Failed to detach kernel driver, error: " + std::to_string(r));
					close();
					return;
				}
			}

			r = libusb_claim_interface(handle, 0);
			if (r != 0)
			{
				LOG_ERROR("Failed to claim interface, error: " + std::to_string(r));
				close();
				return;
			}
			reset();

			if (findEndPoints() != 0)
			{
				LOG_ERROR("Failed to find bulk endpoints");
				close();
				return;
			}

			if (!setBaudRate())
			{
				close();
				return;
			}
			readConfig();

			if (!start())
			{
				close();
				return;
			}
		}
	}

	bool GS_CAN_Interface::read_frame(CANMessageFrame &canFrame)
	{
		if (!handle)
		{
			LOG_ERROR("read_frame: invalid USB device handle");
			return false;
		}

		gs_host_frame frame = {};
		int actual_length = 0;

		int res = libusb_bulk_transfer(
		  handle,
		  ep_in, // IN endpoint, pl. 0x81
		  reinterpret_cast<unsigned char *>(&frame),
		  sizeof(frame),
		  &actual_length,
		  1000 // timeout in ms
		);

		if (res != 0)
		{
			if (res == LIBUSB_ERROR_TIMEOUT)
			{
				LOG_DEBUG("read_frame: timeout, no frame available");
			}
			else
			{
				LOG_ERROR("libusb_bulk_transfer failed: code " + std::to_string(res));
			}
			return false;
		}

		if (actual_length != sizeof(frame))
		{
			LOG_WARNING("read_frame: unexpected frame size " + std::to_string(actual_length));
			return false;
		}

		if (CAN_ERR_FLAG == frame.can_id)
		{
			LOG_WARNING("CAN controller error reported");
			return false;
		}

		if (0x42424242 == frame.echo_id)
		{
			// this is a frame what we sent out
			return false;
		}
		// Decode the received frame
		canFrame.identifier = frame.can_id & 0x1FFFFFFF;
		canFrame.isExtendedFrame = (frame.can_id & (1U << 31)) != 0;

		canFrame.dataLength = frame.can_dlc > 8 ? 8 : frame.can_dlc;
		std::memcpy(canFrame.data, frame.data, canFrame.dataLength);
		canFrame.channel = frame.channel;
		return true;
	}

	bool GS_CAN_Interface::write_frame(const CANMessageFrame &canFrame)
	{
		if (!handle)
		{
			LOG_ERROR("write_frame: invalid USB device handle");
			return false;
		}

		gs_host_frame frame = {};
		frame.echo_id = 0x42424242;
		frame.can_id = canFrame.identifier;
		if (canFrame.isExtendedFrame)
		{
			frame.can_id |= (1U << 31);
		}

		frame.can_dlc = canFrame.dataLength;
		frame.channel = canFrame.channel;
		frame.flags = 0; // no RTR or ECHO or ERR flags
		std::memcpy(frame.data, canFrame.data, canFrame.dataLength);

		int actual_length = 0;
		int res = libusb_bulk_transfer(
		  handle,
		  ep_out, // OUT endpoint (e.g., 0x02)
		  reinterpret_cast<unsigned char *>(&frame),
		  sizeof(frame),
		  &actual_length,
		  1000 // timeout ms
		);

		if (res != 0)
		{
			LOG_ERROR("libusb_bulk_transfer failed: code " + std::to_string(res));
			return false;
		}

		if (actual_length != sizeof(frame))
		{
			LOG_WARNING("Incomplete CAN frame transfer: " + std::to_string(actual_length) + " of " + std::to_string(sizeof(frame)));
			return false;
		}

		return true;
	}

#if defined(ANDROID)
    void GS_CAN_Interface::set_file_descriptor(int descriptor)
    {
        file_descriptor = (intptr_t)descriptor;
        if (openedWithoutFd) {
            open();
            openedWithoutFd = false;
        }
    }
#else
    bool GS_CAN_Interface::set_serial(const std::string &serial)
	{
		bool retVal = false;

		if (!get_is_valid())
		{
			target_serial = serial;
			retVal = true;
		}
		return retVal;
	}
#endif

	bool GS_CAN_Interface::start()
	{
		if (!handle)
		{
			LOG_ERROR("reset_channel: invalid device handle");
			return false;
		}

		gs_device_mode mode_req = {};
		mode_req.mode = htole32(GS_CAN_MODE_START);

		int ret = libusb_control_transfer(
		  handle,
		  LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_OUT,
		  GS_USB_BREQ_MODE,
		  0, // wValue
		  0, // wIndex: CAN interface/channel number
		  reinterpret_cast<uint8_t *>(&mode_req),
		  sizeof(mode_req),
		  1000 // timeout in ms
		);

		if (ret < 0)
		{
			LOG_ERROR("start_channel failed: libusb error " + std::to_string(ret));
			return false;
		}

		if (ret != sizeof(mode_req))
		{
			LOG_WARNING("Incomplete start transfer: " + std::to_string(ret) + " bytes");
			return false;
		}

		return true;
	}

	bool GS_CAN_Interface::readConfig()
	{
		if (!handle)
		{
			LOG_ERROR("Invalid device handle");
			return false;
		}

		struct gs_device_bt_const bt_const;

		int transferred = libusb_control_transfer(
		  handle,
		  LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_IN,
		  GS_USB_BREQ_BT_CONST,
		  0,
		  0,
		  reinterpret_cast<uint8_t *>(&bt_const),
		  sizeof(bt_const),
		  1000);

		if (transferred < 0)
		{
			LOG_ERROR("libusb_control_transfer failed: " + std::to_string(transferred));
			return false;
		}

		if (transferred != sizeof(bt_const))
		{
			LOG_WARNING("Incomplete settings transfer: " + std::to_string(transferred) + " bytes");
			return false;
		}
		return true;
	}

	bool GS_CAN_Interface::reset()
	{
		if (!handle)
		{
			LOG_ERROR("reset_channel: invalid device handle");
			return false;
		}

		gs_device_mode mode_req = {};
		mode_req.mode = htole32(GS_CAN_MODE_RESET);

		int ret = libusb_control_transfer(
		  handle,
		  LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_OUT,
		  GS_USB_BREQ_MODE,
		  0,
		  0,
		  reinterpret_cast<uint8_t *>(&mode_req),
		  sizeof(mode_req),
		  1000);

		if (ret < 0)
		{
			LOG_ERROR("start_channel failed: libusb error " + std::to_string(ret));
			return false;
		}

		if (ret != sizeof(mode_req))
		{
			LOG_WARNING("Incomplete start transfer: " + std::to_string(ret) + " bytes");
			return false;
		}

		return true;
	}

	bool GS_CAN_Interface::setBaudRate()
	{
		if (!handle)
		{
			LOG_ERROR("Invalid device handle");
			return false;
		}

		gs_host_bittiming timing = {};
		timing.prop_seg = htole32(10);
		timing.phase_seg1 = htole32(3);
		timing.phase_seg2 = htole32(2);
		timing.sjw = htole32(1);
		timing.brp = htole32(12);

		int transferred = libusb_control_transfer(
		  handle,
		  LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_OUT,
		  GS_USB_BREQ_BITTIMING,
		  0,
		  0,
		  reinterpret_cast<uint8_t *>(&timing),
		  sizeof(timing),
		  1000);

		if (transferred < 0)
		{
			LOG_ERROR("libusb_control_transfer failed: " + std::to_string(transferred));
			return false;
		}

		if (transferred != sizeof(timing))
		{
			LOG_WARNING("Incomplete bittiming transfer: " + std::to_string(transferred) + " bytes");
			return false;
		}

		LOG_INFO("Successfully configured CAN bittiming for 250 kbps");
		return true;
	}

	int GS_CAN_Interface::findEndPoints()
	{
		if (!handle)
			return -1;

		libusb_device *dev = libusb_get_device(handle);
		libusb_config_descriptor *config = nullptr;

		int r = libusb_get_active_config_descriptor(dev, &config);
		if (r != 0 || !config)
		{
			return -2;
		}

		bool found_in = false, found_out = false;

		for (int i = 0; i < config->bNumInterfaces; ++i)
		{
			const libusb_interface &iface = config->interface[i];

			for (int j = 0; j < iface.num_altsetting; ++j)
			{
				const libusb_interface_descriptor &iface_desc = iface.altsetting[j];

				for (int k = 0; k < iface_desc.bNumEndpoints; ++k)
				{
					const libusb_endpoint_descriptor &ep = iface_desc.endpoint[k];
					uint8_t addr = ep.bEndpointAddress;
					uint8_t dir = addr & LIBUSB_ENDPOINT_DIR_MASK;
					uint8_t type = ep.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK;

					if (type == LIBUSB_TRANSFER_TYPE_BULK)
					{
						if (dir == LIBUSB_ENDPOINT_IN && !found_in)
						{
							ep_in = addr;
							found_in = true;
						}
						else if (dir == LIBUSB_ENDPOINT_OUT && !found_out)
						{
							ep_out = addr;
							found_out = true;
						}

						if (found_in && found_out)
						{
							libusb_free_config_descriptor(config);
							return 0;
						}
					}
				}
			}
		}

		libusb_free_config_descriptor(config);
		return -3;
	}
}
