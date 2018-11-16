# ==================================================================================================
# File for detecting and setting up the vulkan library
# ==================================================================================================

# will check the directory, and if it exists
function(vk_get_directory VULKAN_ROOT VULKAN_VERSION output_inc output_lib)
	string(REGEX REPLACE "/$" "" VULKAN_ROOT "${VULKAN_ROOT}")
	if(NOT EXISTS "${VULKAN_ROOT}/${VULKAN_VERSION}" OR NOT IS_DIRECTORY "${VULKAN_ROOT}/${VULKAN_VERSION}")
		if(NOT ${VULKAN_VERSION} EQUAL "")
			if(WIN32)
				message(STATUS "attempting to locate vulkan SDK using common paths")
				set(local_inc "C:/VulkanSDK/${VULKAN_VERSION}/Include")
				set(local_lib "C:/VulkanSDK/${VULKAN_VERSION}/Lib")
			elseif(LINUX)
				message(STATUS "attempting to locate vulkan SDK using common paths")
				if(EXISTS "/vulkan/${VULKAN_VERSION}" AND IS_DIRECTORY "/vulkan/${VULKAN_VERSION}")
					set(local_inc "/vulkan/${VULKAN_VERSION}/x86_64/include")
					set(local_lib "/vulkan/${VULKAN_VERSION}/x86_64/lib")
				else()
					set(local_inc "/VulkanSDK/${VULKAN_VERSION}/x86_64/include")
					set(local_lib "/VulkanSDK/${VULKAN_VERSION}/x86_64/lib")
				endif()
			else()
				message(FATAL_ERROR "trying to include vulkan on an unsupported platform")
			endif()
		else()
			message(STATUS "auto-locating vulkan SDK using CMake...")
			find_package(Vulkan)
			
			# Try extracting VulkanSDK path from ${Vulkan_INCLUDE_DIRS}
			if (NOT ${Vulkan_INCLUDE_DIRS} STREQUAL "")
				set(local_inc ${Vulkan_INCLUDE_DIRS})
				STRING(REGEX REPLACE "/Include" "" local_inc ${local_inc})
			endif()
			
			if(NOT Vulkan_FOUND AND NOT EXISTS "${local_inc}")
				message(FATAL_ERROR "could not locate vulkan, nor the include path on this system, please set it manually")
			endif()
		endif()
	else()
		set(local_inc "${VULKAN_ROOT}/${VULKAN_VERSION}")
		if(WIN32)
			set(local_lib "${local_inc}/Lib")
			set(local_inc "${local_inc}/Include")
		elseif(LINUX)
			set(local_lib "${local_inc}/x86_64/lib")
			set(local_inc "${local_inc}/x86_64/include")		
		endif()
	endif()
	
	if(NOT EXISTS "${local_inc}" OR NOT IS_DIRECTORY "${local_inc}")
		message(FATAL_ERROR "could not locate vulkan, nor the include path (tried ${local_inc}) on this system using the version [${VULKAN_VERSION}], please set 'VULKAN_ROOT' manually to the root directory of the SDK, and the 'VULKAN_VERSION' to the version of vulkan you wish to include")
	endif()
	set(${output_inc} "${local_inc}" PARENT_SCOPE)
	set(${output_lib} "${local_lib}" PARENT_SCOPE)
endfunction()

function(setup_vulkan root version out_inc_path out_lib_path out_lib_name)
	if(NOT version)
		set(version "default")
	endif()
	vk_get_directory("${root}" "${version}" VK_INCPATH VK_LIBPATH)
	
	if(${VULKAN_STATIC})
		set(${out_inc_path} "${VK_INCPATH}" PARENT_SCOPE)
	   link_directories(${VK_LIBPATH})
	   
	   set(${out_lib_path} "${VK_LIBPATH}" PARENT_SCOPE)
	
		if(WIN32)
			set(${out_lib_name} "vulkan-1" PARENT_SCOPE)
		elseif(LINUX)
			set(${out_lib_name} "libvulkan.so" PARENT_SCOPE)
		else()
			message(FATAL_ERROR "unsupported platform")
		endif()
	else()
		set(${out_inc_path} "${VK_INCPATH}" "" PARENT_SCOPE )	
	endif()	
endfunction()