#pragma once
#include "fwd/gles/shader.h"
#include "fwd/resource/resource.h"
#include "gles/types.h"
#include "psl/ustring.h"

namespace core::igles
{
	class shader
	{
	  public:
		shader(core::resource::cache& cache, const core::resource::metadata& metaData, core::meta::shader* metaFile);
		~shader();
		shader(const shader&) = delete;
		shader(shader&&)	  = delete;
		shader& operator=(const shader&) = delete;
		shader& operator=(shader&&) = delete;

		GLuint id() const noexcept;
		core::meta::shader* meta() const noexcept { return m_Meta; }

	  private:
		GLuint m_Shader {std::numeric_limits<GLuint>::max()};
		core::meta::shader* m_Meta;
	};
}	 // namespace core::igles