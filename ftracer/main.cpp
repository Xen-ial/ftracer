#include "FWatcher.h"

#include <system_error>
#include <span>
#include <print>

namespace wdContainer
{
	void startWatchdog(const char* mountDir);
}

int main(int argc, char* argv[])
{
	auto args = std::span(argv, argc);

	if (args.size() < 2)
	{
		std::println("Usage: MOUNT {}.", args[0]);
		return 1;
	}

	// Starts the watchdog
	wdContainer::startWatchdog(args[1]);

	return 0;
}

void wdContainer::startWatchdog(const char* mountDir)
{
	// Create new watchdog
	fwatcher::Watchdog watchdog;

	// Fill the flags we want to attach to
	fwatcher::FwFlags flags{};

	flags.markFlags = FAN_MARK_ADD | FAN_MARK_MOUNT;
	flags.markMask  = FAN_OPEN_PERM | FAN_CLOSE_WRITE | FAN_MODIFY;

	flags.initFlags      = FAN_CLOEXEC | FAN_CLASS_CONTENT | FAN_NONBLOCK;
	flags.initEventFlags = O_RDONLY | O_LARGEFILE;

	watchdog.setFlags(flags);

	try
	{
		// Initalize the watchdog at a specified directory
		watchdog.init(mountDir);
		while (true)
		{
			// Poll the new events from the stack
			if (!watchdog.pollEvents())
				continue;

			// Read the next event available
			if (!watchdog.readNext())
				continue;

			// Loop through all events in the payload
			for (const auto& event : watchdog.getEvents())
			{
				// Check the fanotify event version
				if (!fwatcher::checkVers(event))
					continue;

				// Ensure the event is valid before we handle it
				if (!fwatcher::validEvent(event)) continue;

				// Check if the event is open perm
				if (fwatcher::checkMask(event, FAN_OPEN_PERM))
				{
					std::print("Opened File: ");

					// Respond to the event alowing access permissions
					watchdog.sendResponse(event, FAN_ALLOW);
				}

				if (fwatcher::checkMask(event, FAN_MODIFY))
					std::print("Modified File: ");

				if (fwatcher::checkMask(event, FAN_CLOSE_WRITE))
					std::print("Closed File: ");

				// Get the files path from the current event
				std::string path = fwatcher::getPath(event);
				std::println("{}", path);

				// Clean up events
				fwatcher::closeEvent(event);
			}
		}
	}
	catch (const std::system_error& e)
	{
		std::println("Error: {}\n{}", e.code().message(), e.what());
	}
}