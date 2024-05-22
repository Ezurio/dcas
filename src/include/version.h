
#if !defined(DCAS_VERSION_STRING)
	#error "error: undefined development version - please define via build system"
#endif

#define SUMMIT_BUILD_NUMBER "12.0.0.7"

#ifndef SDC_SDK_MSB
#error "error: API version defines not present"
#endif

//the component value should not change
#define COMPONENT    92

// the following #DEFINES should not be modified by hand
// -----------------------------------------------------
#define STR_VALUE(arg) #arg
#define ZVER(name) STR_VALUE(name)

#define DCAL_VERSION_STR ZVER(SDC_SDK_MSB) "." ZVER(DCAL_MAJOR) "." ZVER(DCAL_MINOR)
#define DCAL_VERSION ((SDC_SDK_MSB << 16) | (DCAL_MAJOR << 8) | DCAL_MINOR)
#define DCAS_COMPONENT_VERSION ((COMPONENT << 24) | (SDC_SDK_MSB << 16) | (DCAL_MAJOR << 8) | DCAL_MINOR)
// -----------------------------------------------------
