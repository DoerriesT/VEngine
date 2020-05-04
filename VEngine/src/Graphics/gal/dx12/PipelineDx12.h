#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include <d3d12.h>
#include "Utility/ObjectPool.h"
#include "DescriptorSetDx12.h"

namespace VEngine
{
	namespace gal
	{
		struct GraphicsPipelineCreateInfo;
		struct ComputePipelineCreateInfo;

		struct DescriptorTableLayout
		{
			D3D12_DESCRIPTOR_RANGE_TYPE m_rangeTypes[32];
			uint32_t m_descriptorCount[32];
			uint32_t m_offsetFromTableStart[32];
			uint32_t m_totalDescriptorCount;
			uint32_t m_rangeCount;
		};

		struct DescriptorSetLayoutsDx12
		{
			enum { MAX_SET_COUNT = 4 };
			StaticObjectPool<ByteArray<sizeof(DescriptorSetLayoutDx12)>, MAX_SET_COUNT> m_descriptorSetLayoutMemoryPool;
			DescriptorSetLayoutDx12 *m_descriptorSetLayouts[MAX_SET_COUNT];
			uint32_t m_layoutCount;
		};

		class GraphicsPipelineDx12 : public GraphicsPipeline
		{
		public:
			explicit GraphicsPipelineDx12(ID3D12Device *device, const GraphicsPipelineCreateInfo &createInfo);
			GraphicsPipelineDx12(GraphicsPipelineDx12 &) = delete;
			GraphicsPipelineDx12(GraphicsPipelineDx12 &&) = delete;
			GraphicsPipelineDx12 &operator= (const GraphicsPipelineDx12 &) = delete;
			GraphicsPipelineDx12 &operator= (const GraphicsPipelineDx12 &&) = delete;
			~GraphicsPipelineDx12();
			void *getNativeHandle() const override;
			uint32_t getDescriptorSetLayoutCount() const override;
			const DescriptorSetLayout *getDescriptorSetLayout(uint32_t index) const override;
			ID3D12RootSignature *getRootSignature() const;
			uint32_t getDescriptorTableOffset() const;
			uint32_t getVertexBufferStride(uint32_t bufferBinding) const;
			void initializeState(ID3D12GraphicsCommandList *cmdList) const;

		private:
			ID3D12PipelineState *m_pipeline;
			ID3D12RootSignature *m_rootSignature;
			DescriptorSetLayoutsDx12 m_descriptorSetLayouts;
			DescriptorTableLayout m_descriptorTableLayouts[4];
			uint32_t m_descriptorTableOffset;
			uint32_t m_descriptorTableCount;
			UINT m_vertexBufferStrides[32];
			float m_blendFactors[4];
			UINT m_stencilRef;
		};

		class ComputePipelineDx12 : public ComputePipeline
		{
		public:
			explicit ComputePipelineDx12(ID3D12Device *device, const ComputePipelineCreateInfo &createInfo);
			ComputePipelineDx12(ComputePipelineDx12 &) = delete;
			ComputePipelineDx12(ComputePipelineDx12 &&) = delete;
			ComputePipelineDx12 &operator= (const ComputePipelineDx12 &) = delete;
			ComputePipelineDx12 &operator= (const ComputePipelineDx12 &&) = delete;
			~ComputePipelineDx12();
			void *getNativeHandle() const override;
			uint32_t getDescriptorSetLayoutCount() const override;
			const DescriptorSetLayout *getDescriptorSetLayout(uint32_t index) const override;
			//const DescriptorSetLayout *getDescriptorSetLayout(uint32_t index) const override;
			ID3D12RootSignature *getRootSignature() const;
			uint32_t getDescriptorTableOffset() const;

		private:
			ID3D12PipelineState *m_pipeline;
			ID3D12RootSignature *m_rootSignature;
			DescriptorSetLayoutsDx12 m_descriptorSetLayouts;
			DescriptorTableLayout m_descriptorTableLayouts[4];
			uint32_t m_descriptorTableOffset;
			uint32_t m_descriptorTableCount;
		};
	}
}