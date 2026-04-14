#pragma once

#include <vector>
#include <string>
#include <filesystem>

#include <utility>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/fanotify.h>

namespace fwatcher
{
    /**
     * File Watcher flags, contains FANotify mark flags, and init flags
     */
    struct fw_flags
    {
        unsigned int mark_flags;
        unsigned int mark_mask;

        unsigned int init_flags;
        unsigned int init_event_flags;
    };

    /**
     * File Descriptor
     */
    class descriptor
    {
    public:
        descriptor() = default;
        explicit descriptor(int fd) : fd_(fd) {}

        descriptor(descriptor &&other) noexcept
        {
            this->fd_ = std::exchange(other.fd_, -1);
        }

        descriptor(const descriptor &other) = delete;
        descriptor &operator=(const descriptor &other) = delete;

        descriptor &operator=(descriptor &&other) noexcept
        {
            if (&other == this)
                return *this;

            if (this->fd_ != -1)
                close(this->fd_);

            this->fd_ = std::exchange(other.fd_, -1);
            return *this;
        }

        ~descriptor()
        {
            if (fd_ == -1)
                return;

            close(fd_);
        }

        int get_fd() const
        {
            return fd_;
        }

    private:
        int fd_ = -1;
    };

    /**
     * Event iterator
     */
    struct fanotify_iterator
    {
        const fanotify_event_metadata *ptr;
        size_t remaining_size;

        const fanotify_event_metadata &operator*() const
        {
            return *ptr;
        }

        fanotify_iterator &operator++()
        {
            ptr = FAN_EVENT_NEXT(ptr, remaining_size);
            return *this;
        }

        bool operator!=(const fanotify_iterator &other) const
        {
            return FAN_EVENT_OK(ptr, remaining_size);
        }
    };

    /**
     * Holds event data in a way that is iteratable
     */
    class read_events
    {
    public:
        read_events(const fanotify_event_metadata *p, size_t s) : start_ptr(p), total_size(s) {}

        fanotify_iterator begin() const
        {
            return {start_ptr, total_size};
        }

        fanotify_iterator end() const
        {
            return {nullptr, 0};
        }

    private:
        const fanotify_event_metadata *start_ptr;
        size_t total_size;
    };

    /**
     * Watchdog for watching files via fanotify API
     */
    class watchdog
    {
    public:
        watchdog() : buf_(BUF_SIZE) {}
        watchdog(const watchdog &other) = delete;

        fw_flags flags;

        /**
         * Init watchdog
         * @param mounting directory
         */
        void init(const std::string &mount);

        /**
         * Reads the next set of events
         */
        bool read_next();

        /**
         * Sends a response to permission based events
         * @param event takes fanotify event
         * @param response takes a fanotify permission response
         */
        void send_response(const fanotify_event_metadata &event, unsigned int response);

        /**
         * Finalizes the events and cleans them up
         * @param event takes fanotify event
         */
        void close_event(const fanotify_event_metadata &event) const;

        /**
         * Grabs new events
         */
        bool poll_events();

        /**
         * Gets all events in an iterator format
         */
        read_events get_events() const;

        /**
         * Checks if the event version matches the current
         * @param event takes fanotify event
         */
        bool check_vers(const fanotify_event_metadata &event) const;

        /**
         * Checks if the current mask contains the eventID
         * @param event takes fanotify event
         * @param event_id takes fanotify mask event_id
         */
        bool check_mask(const fanotify_event_metadata &event, unsigned int event_id) const;

        /**
         * Checks if the current event is valid
         * @param event takes fanotify event
         */
        bool valid_event(const fanotify_event_metadata &event) const;

        /**
         * Gets the file path that has been touched
         * @param event takes fanotify event
         */
        std::string get_path(const fanotify_event_metadata &event);

        /**
         * Gets raw fanotify metadata buffer
         */
        char *get_buffer()
        {
            return buf_.data();
        }

        /**
         * Gets raw fanotify metadata size
         */
        size_t &get_size()
        {
            return size_;
        }

    private:
        static constexpr size_t BUF_SIZE = 4096;
        std::vector<char> buf_;

        struct fanotify_response response_;
        size_t size_;

        descriptor fanotif_fd_;

        char poll_buf_;
        int poll_num_;

        std::array<pollfd, 2> fds_{};
    };
}