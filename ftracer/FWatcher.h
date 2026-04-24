#pragma once

#include <vector>
#include <array>
#include <string>

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
	struct FwFlags
	{
		unsigned int markFlags;
		unsigned int markMask;

		unsigned int initFlags;
		unsigned int initEventFlags;
	};

	/**
	 * File Descriptor
	 */
	class Descriptor
	{
	public:
		Descriptor() = default;
		explicit Descriptor(int fileDescriptor) : m_fd(fileDescriptor) {}

		Descriptor(Descriptor&& other) noexcept
		{
			this->m_fd = std::exchange(other.m_fd, -1);
		}

		Descriptor(const Descriptor& other)            = delete;
		Descriptor& operator=(const Descriptor& other) = delete;

		Descriptor& operator=(Descriptor&& other) noexcept
		{
			if (&other == this)
				return *this;

			if (this->m_fd != -1)
				close(this->m_fd);

			this->m_fd = std::exchange(other.m_fd, -1);
			return *this;
		}

		~Descriptor()
		{
			if (m_fd == -1)
				return;

			close(m_fd);
		}

		[[nodiscard]] int getFd() const
		{
			return m_fd;
		}

	private:
		int m_fd = -1;
	};

	/**
	 * Event iterator
	 */
	struct FanotifyIterator
	{
		const fanotify_event_metadata* ptr;
		size_t                         remainingSize;

		const fanotify_event_metadata& operator*() const
		{
			return *ptr;
		}

		FanotifyIterator& operator++()
		{
			ptr = FAN_EVENT_NEXT(ptr, remainingSize);
			return *this;
		}

		bool operator!=(const FanotifyIterator& other) const
		{
			return FAN_EVENT_OK(ptr, remainingSize);
		}
	};

	/**
	 * Holds event data in a way that is iteratable
	 */
	class ReadEvents
	{
	public:
		ReadEvents(const fanotify_event_metadata* ptr, size_t size) : m_startPtr(ptr), m_totalSize(size) {}

		[[nodiscard]] FanotifyIterator begin() const
		{
			return {.ptr = m_startPtr, .remainingSize = m_totalSize};
		}

		static FanotifyIterator end()
		{
			return {.ptr = nullptr, .remainingSize = 0};
		}

	private:
		const fanotify_event_metadata* m_startPtr;
		size_t                         m_totalSize;
	};

	/**
	 * Checks if the event version matches the current
	 * @param event takes fanotify event
	 */
	bool checkVers(const fanotify_event_metadata& event);

	/**
	 * Checks if the current mask contains the eventID
	 * @param event takes fanotify event
	 * @param event_id takes fanotify mask event_id
	 */
	bool checkMask(const fanotify_event_metadata& event, unsigned int eventId);

	/**
	 * Checks if the current event is valid
	 * @param event takes fanotify event
	 */
	bool validEvent(const fanotify_event_metadata& event);

	/**
	 * Gets the file path that has been touched
	 * @param event takes fanotify event
	 */
	std::string getPath(const fanotify_event_metadata& event);

	/**
	 * Finalizes the events and cleans them up
	 * @param event takes fanotify event
	 */
	void closeEvent(const fanotify_event_metadata& event);

	/**
	 * Watchdog for watching files via fanotify API
	 */
	class Watchdog
	{
	public:
		Watchdog() : m_buf(BUF_SIZE) {}
		Watchdog(Watchdog&&)                 = delete;
		Watchdog& operator=(const Watchdog&) = delete;
		Watchdog& operator=(Watchdog&&)      = delete;
		Watchdog(const Watchdog& other)      = delete;

		~Watchdog() = default;

		/**
		 * Init watchdog
		 * @param mounting directory
		 */
		void init(const std::string& mount);

		/**
		 * Reads the next set of events
		 */
		bool readNext();

		/**
		 * Sends a response to permission based events
		 * @param event takes fanotify event
		 * @param response takes a fanotify permission response
		 */
		void sendResponse(const fanotify_event_metadata& event, unsigned int response);

		/**
		 * Grabs new events
		 */
		bool pollEvents();

		/**
		 * Gets all events in an iterator format
		 */
		[[nodiscard]] ReadEvents getEvents() const;

		/**
		 * Gets raw fanotify metadata buffer
		 */
		char* getBuffer() { return m_buf.data(); }

		/**
		 * Gets raw fanotify metadata size
		 */
		size_t& getSize() { return m_size; }

		/**
		 * Gets the watchdog flags
		 */
		[[nodiscard]] FwFlags getFlags() const { return m_flags; }

		/**
		 * Sets the watchdog flags
		 */
		void setFlags(FwFlags flags) { m_flags = flags; }

	private:
		static constexpr size_t BUF_SIZE = 4096;
		std::vector<char>       m_buf;

		FwFlags                  m_flags{};
		struct fanotify_response m_response{};
		size_t                   m_size{};

		Descriptor m_fanotifFd;

		char m_pollBuf{};
		int  m_pollNum{};

		std::array<pollfd, 2> m_fds{};
	};
} // namespace fwatcher