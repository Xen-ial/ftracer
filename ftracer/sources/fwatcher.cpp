#include "fwatcher.h"

#include <print>
#include <system_error>

bool fwatcher::watchdog::read_next()
{
    ssize_t result = read(fanotif_fd_.get_fd(), buf_.data(), buf_.size());
    if (result == -1 && errno != EAGAIN)
        throw std::system_error(errno, std::generic_category(), "fanotify read");

    size_ = static_cast<size_t>(result);
    return size_ > 0;
}

std::string fwatcher::watchdog::get_path(const fanotify_event_metadata &event)
{
    std::error_code ec;
    std::string procfd_path = std::format("/proc/self/fd/{}", event.fd);
    std::string path = std::filesystem::read_symlink(procfd_path, ec);
    return ec ? std::string() : path;
}

void fwatcher::watchdog::send_response(const fanotify_event_metadata &event, unsigned int response)
{
    response_.fd = event.fd;
    response_.response = response;
    write(fanotif_fd_.get_fd(), &response_, sizeof response_);
}

fwatcher::read_events fwatcher::watchdog::get_events() const
{
    return fwatcher::read_events(reinterpret_cast<const fanotify_event_metadata *>(buf_.data()), size_);
}

bool fwatcher::watchdog::poll_events()
{
    poll_num_ = poll(fds_.data(), fds_.size(), -1);
    if (poll_num_ == -1)
    {
        if (errno == EINTR)
            return false;

        throw std::system_error(errno, std::generic_category(), "poll");
    }

    if (poll_num_ > 0)
    {
        if (fds_[0].revents & POLLIN)
        {
            while (read(STDIN_FILENO, &poll_buf_, 1) > 0 && poll_buf_ != '\n')
                continue;

            return false;
        }

        if (fds_[1].revents & POLLIN)
            return true;
    }

    return false;
}

void fwatcher::watchdog::init(const std::string &mount)
{
    descriptor fanotif_fd(fanotify_init(flags.init_flags, flags.init_event_flags));
    fanotif_fd_ = std::move(fanotif_fd);

    if (fanotif_fd_.get_fd() == -1)
        throw std::system_error(errno, std::generic_category(), "fanotify_init");

    if (fanotify_mark(fanotif_fd_.get_fd(), flags.mark_flags, flags.mark_mask, AT_FDCWD, mount.c_str()) == -1)
        throw std::system_error(errno, std::generic_category(), "fanotify_mark");

    fds_[0].fd = STDIN_FILENO;
    fds_[0].events = POLLIN;

    fds_[1].fd = fanotif_fd_.get_fd();
    fds_[1].events = POLLIN;
}

void fwatcher::watchdog::close_event(const fanotify_event_metadata &event) const
{
    if (event.fd == -1)
        return;

    close(event.fd);
}

bool fwatcher::watchdog::check_vers(const fanotify_event_metadata &event) const
{
    return event.vers == FANOTIFY_METADATA_VERSION;
}

bool fwatcher::watchdog::check_mask(const fanotify_event_metadata &event, unsigned int event_id) const
{
    return event.mask & event_id;
}

bool fwatcher::watchdog::valid_event(const fanotify_event_metadata &event) const
{
    return event.fd >= 0;
}
