# ftracer

ftracer was made to make watching mount directories for file events, eventually it will expand to allowing you to watch processes, and filter for specific files.  

## Details

Modern approach at FANotify for linux systems. Created for simplicity for interacting with mount directories.  

> This library is a work in progress

## fwatcher namepsace

**File Watcher Flags:**

```cpp
struct FwFlags
{
    unsigned int markFlags;
    unsigned int markMask;

    unsigned int initFlags;
    unsigned int initEventFlags;
}
```

These flags are used for initialization, and mark setup.

- **markFlags:** Marks on filesystem objects
- **markMask:** Marks for events that happened on a filesystem object
- **initFlags:** Notification Class for the listening application
- **initEventFlags:** Behaviour of the file descriptor

These flags need to be initialized first before running `watchdog.init(mount)`

***Useful Links:***

[MARK FLAGS](https://man7.org/linux/man-pages/man2/fanotify_mark.2.html)
[INIT FLAGS](https://man7.org/linux/man-pages/man2/fanotify_init.2.html)
[FANOTIFY](https://man7.org/linux/man-pages/man7/fanotify.7.html)

***Functions:***

```cpp
bool checkVers(const fanotify_event_metadata& event);
bool checkMask(const fanotify_event_metadata& event, unsigned int eventId);
bool validEvent(const fanotify_event_metadata& event);
std::string getPath(const fanotify_event_metadata& event);
void closeEvent(const fanotify_event_metadata& event);
```

Function Descriptions:

- **checkVers:** Makes sure the event matches the FANotify version.
- **checkMask:** Checks if the event is the same as the mask.
- **validEvent:** Ensures the event is a valid event in the buffer.
- **getPath:** Gets the current events file path.
- **closeEvent:** Cleans up and closes the event so it doesnt leak.

**ReadEvents:**

This class holds fanotify event data in a way that allows it to be iteratable.

Constructor:

```cpp
ReadEvents(const fanotify_event_metadata *ptr, size_t size);
```

Example Usage:

```cpp
for (const auto &event : ReadEvents(reinterpret_cast<const fanotify_event_metadata *>(Watchdog::getBuffer()), Watchdog::getSize()))
```

This class doesnt need to be used if your using watchdog as it contains `Watchdog::getEvents()` which handles collecting all events

**Descriptor:**

This class holds the file descriptor. In a RAII manner, for auto disposal on leaving scope.

> Descriptor class can not be copied, only moved via std::move() due to file descriptor safety

```cpp
Descriptor(int fd);
```

Example Usage:

```cpp
Descriptor fileDesc(fanotify_init(flags.initFlags, flags.initEventFlags));
```

### Watchdog

Containing class for setting up directory/mount watching.

> Watchdog class cannot be copied.

```cpp
void init(const std::string& mount);
bool pollEvents();
bool readNext();
ReadEvents getEvents();
void sendResponse(const fanotify_event_metadata& event, unsigned int response);
char* getBuffer();
size_t& getSize();
FwFlags getFlags();
void setFlags(FwFlags flags);
```

Function Description:

- **init:** Initialzes watchdog at a directory/mount (sets up resources.)
- **pollEvents:** Collects all events from fantoify event buffer.
- **readNext:** Reads the next set of events from the buffer.
- **getEvents:** Collects all events into an iterator and allows ease of use to read them.
- **sendResponse:** Sends a response back to the fanotify api allowing/not-allowing a file or event to have permissions to complete.
- **getBuffer:** Gets the buffer currently being used.
- **getSize:** Gets the size of the current event data buffer.
- **getFlags:** Gets the flags used for watchdog, init and masking.
- **setFlags:** Sets the flags for watchdog, init and masking.

## Example

How to use watchdog effectively -

Initialize class:

```cpp
fwatcher::Watchdog watchdog;
```

Fill Flags:

```cpp
fwatcher::FwFlags flags{};

flags.markFlags = FAN_MARK_ADD | FAN_MARK_MOUNT;
flags.markMask  = FAN_OPEN_PERM | FAN_CLOSE_WRITE | FAN_MODIFY;

flags.initFlags      = FAN_CLOEXEC | FAN_CLASS_CONTENT | FAN_NONBLOCK;
flags.initEventFlags = O_RDONLY | O_LARGEFILE;

watchdog.setFlags(flags);
```

> These flags can be found above in the linked man-pages website

Initialize Watchdog:

```cpp
watchdog.init("/");
```

> Set the directory it is going to be watching for file usages

Create loop and filter files:

```cpp
bool isRunning = true;
// Initalize the watchdog at a specified directory
watchdog.init(mountDir);
while (isRunning)
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
        fwatcher::Watchdog::closeEvent(event);
    }
}
```

> Note watchdog does throw errors, so its best to wrap your loop in a try catch  
> Make sure to catch (const std::system_error &e)  
> Check main.cpp for a full example of usage  

## Example Output

```bash
~> sudo ./build/FTracer ~/

Opened File: /usr/lib/localsearch-3.0/extract-modules/libextract-msoffice-xml.so
Opened File: /usr/lib/localsearch-3.0/extract-modules/libextract-libav.so
Opened File: /usr/lib/libavformat.so.62.3.100
Opened File: /usr/lib/libcue.so.2.3.0
Opened File: /usr/lib/libgupnp-dlna-2.0.so.4.0.0
Opened File: /usr/lib/libavcodec.so.62.11.100
Opened File: /usr/lib/libavutil.so.60.8.100
Opened File: /usr/lib/libdvdnav.so.4.4.0
Opened File: /usr/lib/libdvdread.so.8.1.0
```

## How to build

```bash
git clone git@github.com:Xen-ial/ftracer.git
```

Run: `make` within the directory that its been cloned to.  
Then run: `./FTracer mounting_dir`  
Example: `./FTracer ~/` this will run on my home directory.  

**If you wish to contribute to this repo. Feel free to create a pr.**
