#include "fwatcher.h"

#include <system_error>
#include <print>

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::println("Usage: MOUNT {}.", argv[0]);
        return 1;
    }

    // Create new watchdog
    fwatcher::watchdog wd;

    // Fill the flags we want to attach tp
    wd.flags.mark_flags = FAN_MARK_ADD | FAN_MARK_MOUNT;
    wd.flags.mark_mask = FAN_OPEN_PERM | FAN_CLOSE_WRITE | FAN_MODIFY;

    wd.flags.init_flags = FAN_CLOEXEC | FAN_CLASS_CONTENT | FAN_NONBLOCK;
    wd.flags.init_event_flags = O_RDONLY | O_LARGEFILE;

    try
    {
        // Initalize the watchdog at a specified directory
        wd.init(argv[1]);
        while (true)
        {
            // Poll the new events from the stack
            if (!wd.poll_events())
                continue;

            // Read the next event available
            if (!wd.read_next())
                continue;

            // Loop through all events in the payload
            for (const auto &event : wd.get_events())
            {
                // Check the fanotify event version
                if (!wd.check_vers(event))
                    continue;

                // Ensure the event is valid before we handle it
                if (wd.valid_event(event))
                {
                    // Check if the event is open perm
                    if (wd.check_mask(event, FAN_OPEN_PERM))
                    {
                        std::print("Opened File: ");

                        // Respond to the event alowing access permissions
                        wd.send_response(event, FAN_ALLOW);
                    }

                    if (wd.check_mask(event, FAN_MODIFY))
                        std::print("Modified File: ");

                    if (wd.check_mask(event, FAN_CLOSE_WRITE))
                        std::print("Closed File: ");

                    // Get the files path from the current event
                    std::string path = wd.get_path(event);
                    std::println("{}", path);

                    // Clean up events
                    wd.close_event(event);
                }
            }
        }
    }
    catch (const std::system_error &e)
    {
        std::println("Error: {}\n{}", e.code().message(), e.what());
    }

    return 0;
}