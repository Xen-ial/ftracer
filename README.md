# ftracer

Modern approach at FANotify for linux systems. Created for simplicity for interacting with mount directories.

**This library is a work in progress.**

## API

***Namespace: fwatcher***

**File Watcher Flags:**

```cpp
struct fw_flags
{
    unsigned int mark_flags;
    unsigned int mark_mask;

    unsigned int init_flags;
    unsigned int init_event_flags;
}
```

These flags are used for initialization, and mark setup.

- **mark_flags:** Marks on filesystem objects
- **mark_mask:** Marks for events that happened on a filesystem object
- **init_flags:** Notification Class for the listening application
- **init_event_flags:** Behaviour of the file descriptor

These flags need to be initialized first before running `watchdog.init(mount)`

[MARK FLAGS](https://man7.org/linux/man-pages/man2/fanotify_mark.2.html)
[INIT FLAGS](https://man7.org/linux/man-pages/man2/fanotify_init.2.html)
[FANOTIFY](https://man7.org/linux/man-pages/man7/fanotify.7.html)

**Watchdog:**

Containing class for setting up directory/mount watching.

> Watchdog class cannot be copied.

```cpp
void watchdog::init(std::string &mount);
bool watchdog::poll_events();
bool watchdog::read_next();
fwatcher::read_events watchdog::get_events();
bool watchdog::check_vers(fanotify_event_metadata &event);
bool watchdog::valid_event(fanotify_event_metadata &event);
bool watchdog::check_mask(fanotify_event_metadata &event, unsigned int event_id);
void watchdog::send_response(fanotify_event_metadata &event, unsigned int response_id);
std::string watchdog::get_path(fanotify_event_metadata &event);
void watchdog::close_event(fanotify_event_metadata &event);
char *get_buffer();
size_t &get_size();
```

Function Description:

- **init:** Initialzes watchdog at a directory/mount (sets up resources.)
- **poll_events:** Collects all events from fantoify event buffer.
- **read_next:** Reads the next set of events from the buffer.
- **get_events:** Collects all events into an iterator and allows ease of use to read them.
- **check_vers:** Makes sure the event matches the FANotify version.
- **valid_event:** Ensures the event is a valid event in the buffer.
- **check_mask:** Checks if the event is the same as the mask.
- **send_response:** Sends a response back to the fanotify api allowing/not-allowing a file or event to have permissions to complete.
- **get_path:** Gets the current events file path.
- **close_event:** Cleans up and closes the event so it doesnt leak.
- **get_buffer:** Gets the buffer currently being used.
- **get_size:** Gets the size of the current event data buffer.

**read_events:**

This class holds fanotify event data in a way that allows it to be iteratable.

Constructor:

```cpp
read_events(const fanotify_event_metadata *p, size_t s);
```

Example Usage:

```cpp
for (const auto &event : read_events(reinterpret_cast<const fanotify_event_metadata *>(watchdog::get_buffer()), watchdog::get_size()))
```

This class doesnt need to be used if your using watchdog as it contains `watchdog::get_events()` which handles collecting all events

**descriptor:**

This class holds the file descriptor. In a RAII manner, for auto disposal on leaving scope.

> Descriptor class can not be copied, only moved via std::move() due to file descriptor safety

```cpp
descriptor(int fd);
```

Example Usage:

```cpp
descriptor file_descriptor(fanotify_init(flags.init_flags, flags.init_event_flags));
```

## Example

How to use watchdog effectively -

Initialize class:

```cpp
fwatcher::watchdog wd;
```

Fill Flags:

```cpp
wd.flags.mark_flags = FAN_MARK_ADD | FAN_MARK_MOUNT;
wd.flags.mark_mask = FAN_OPEN_PERM | FAN_CLOSE_WRITE | FAN_MODIFY;

wd.flags.init_flags = FAN_CLOEXEC | FAN_CLASS_CONTENT | FAN_NONBLOCK;
wd.flags.init_event_flags = O_RDONLY | O_LARGEFILE;
```

> These flags can be found above in the linked man-pages website

Initialize Watchdog:

```cpp
wd.init("/");
```

> Set the directory it is going to be watching for file usages

Create loop and filter files:

```cpp
bool is_running = true;
while (is_running)
{
    // Poll the new events from the buffer
    if (!wd.poll_events())
        continue;

    // Read the next set of events available
    if (!wd.read_next())
        continue;

    // Loop through all events in the payload
    for (const auto &event : wd.get_events())
    {
        // Always check the version first
        if (!wd.check_vers(event))
            continue;

        // Ensure the event is valid before we handle it
        if (wd.valid_event(event))
        {
            // Check if the event is open perm
            if (wd.check_mask(event, FAN_OPEN_PERM))
            {
                std::print("Opened File: ");

                // Then respond to the event alowing access permissions
                wd.send_response(event, FAN_ALLOW);
            }

            // Get the files path from the current event
            std::string path = wd.get_path(event);
            std::println("{}", path);

            // Then close the event
            wd.close_event(event);
        }
    }
}
```

> Note watchdog does throw errors, so its best to wrap your loop in a try catch  
> Make sure to catch (const std::system_error &e)  
> Check main.cpp for a full example of usage  

## How to build

```bash
git clone git@github.com:Xen-ial/ftracer.git
```

Run: `make` within the directory that its been cloned to.  
Then run: `./FTracer mounting_dir`  
Example: `./FTracer ~/` this will run on my home directory.  

**If you wish to contribute to this repo. Feel free to create a pr.**
