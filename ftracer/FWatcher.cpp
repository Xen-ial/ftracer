#include "FWatcher.h"

#include <system_error>
#include <filesystem>

bool fwatcher::Watchdog::readNext()
{
	ssize_t result = read(m_fanotifFd.getFd(), m_buf.data(), m_buf.size());
	if (result == -1 && errno != EAGAIN)
		throw std::system_error(errno, std::generic_category(), "fanotify read");

	m_size = static_cast<size_t>(result);
	return m_size > 0;
}

void fwatcher::Watchdog::sendResponse(const fanotify_event_metadata& event, unsigned int response)
{
	m_response.fd       = event.fd;
	m_response.response = response;
	write(m_fanotifFd.getFd(), &m_response, sizeof m_response);
}

fwatcher::ReadEvents fwatcher::Watchdog::getEvents() const
{
	const void* rawptr = m_buf.data();
	const auto* eventPtr =
	    static_cast<const fanotify_event_metadata*>(rawptr);

	return {eventPtr, m_size};
}

bool fwatcher::Watchdog::pollEvents()
{
	m_pollNum = poll(m_fds.data(), m_fds.size(), -1);
	if (m_pollNum == -1)
	{
		if (errno == EINTR)
			return false;

		throw std::system_error(errno, std::generic_category(), "poll");
	}

	if (m_pollNum > 0)
	{
		if ((m_fds[0].revents & POLLIN) != 0)
		{
			while (read(STDIN_FILENO, &m_pollBuf, 1) > 0 && m_pollBuf != '\n')
				continue;

			return false;
		}

		if ((m_fds[1].revents & POLLIN) != 0)
			return true;
	}

	return false;
}

void fwatcher::Watchdog::init(const std::string& mount)
{
	Descriptor fileDesc(fanotify_init(m_flags.initFlags, m_flags.initEventFlags));
	m_fanotifFd = std::move(fileDesc);

	if (m_fanotifFd.getFd() == -1)
		throw std::system_error(errno, std::generic_category(), "fanotify_init");

	if (fanotify_mark(m_fanotifFd.getFd(), m_flags.markFlags, m_flags.markMask, AT_FDCWD, mount.c_str()) == -1)
		throw std::system_error(errno, std::generic_category(), "fanotify_mark");

	m_fds[0].fd     = STDIN_FILENO;
	m_fds[0].events = POLLIN;

	m_fds[1].fd     = m_fanotifFd.getFd();
	m_fds[1].events = POLLIN;
}

namespace fwatcher
{
	void closeEvent(const fanotify_event_metadata& event)
	{
		if (event.fd == -1)
			return;

		close(event.fd);
	}

	std::string getPath(const fanotify_event_metadata& event)
	{
		std::error_code errCode;
		std::string     procfdPath = std::format("/proc/self/fd/{}", event.fd);
		std::string     path       = std::filesystem::read_symlink(procfdPath, errCode);
		return errCode ? std::string() : path;
	}

	bool checkVers(const fanotify_event_metadata& event)
	{
		return event.vers == FANOTIFY_METADATA_VERSION;
	}

	bool checkMask(const fanotify_event_metadata& event, unsigned int eventId)
	{
		return (event.mask & eventId) != 0U;
	}

	bool validEvent(const fanotify_event_metadata& event)
	{
		return event.fd >= 0;
	}
} // namespace fwatcher
