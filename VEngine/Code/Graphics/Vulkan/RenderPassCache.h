#pragma once
#include "volk.h"
#include <bitset>

namespace VEngine
{
	struct RenderPassDescription
	{
		enum
		{
			MAX_INPUT_ATTACHMENTS = 8,
			MAX_COLOR_ATTACHMENTS = 8,
			MAX_ATTACHMENTS = MAX_INPUT_ATTACHMENTS + MAX_COLOR_ATTACHMENTS + 1, // +1 is for depth attachment
		};

		struct AttachmentDescription
		{
			VkFormat m_format;
			VkSampleCountFlagBits m_samples;
		};

		uint8_t m_attachmentCount = 0;
		AttachmentDescription m_attachments[MAX_ATTACHMENTS] = {};
		uint8_t m_inputAttachmentCount = 0;
		VkAttachmentReference m_inputAttachments[MAX_INPUT_ATTACHMENTS] = {};
		uint8_t m_colorAttachmentCount = 0;
		VkAttachmentReference m_colorAttachments[MAX_COLOR_ATTACHMENTS] = {};
		std::bitset<MAX_COLOR_ATTACHMENTS> m_resolveAttachments;
		bool m_depthStencilAttachmentPresent = false;
		VkAttachmentReference m_depthStencilAttachment = {};
		size_t m_hashValue;

		// cleans up array elements past array count and precomputes a hash value
		void finalize();
	};

	class RenderPassCache
	{

	};
}