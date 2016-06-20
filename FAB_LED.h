////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Fast Addressable LED Library
// Copyright (c)2015 Dan Truong
//
// Licence: limited use is authorized for the purpose of beta testing
//
// The goal of this library is to be very fast, very small and take advantage
// of the relaxed LED timings to allow other work besides bit banging while the
// LEDs are being updated.
// The library allows calling multiple times the same function to repeat
// patterns, and allows inline color palette management to save on memory space
// usage. The palette conversion is done inline as the pixels are being painted.
//
// The bit-banging is being done using GCC built-ins to avoid contrived ASM
// language code blocks support. It should work for any arduino platform of
// 16MHz or more to function.
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#ifndef FAB_LED_H
#define FAB_LED_H

#include <stdint.h>
#include <Arduino.h>

// This code is sensitive to optimizations if you want to cascade function calls,
// so make the IDE compile at -O2 instead of -Os (size).
#pragma GCC optimize ("-O2")

// static_assert is a built-in C++ 11 assert that Arduino does not seem to support
#ifndef static_assert
#define STATIC_ASSERT4(COND,MSG,LIN) typedef char assert_##MSG##_##LIN[(!!(COND))*2-1]
#define STATIC_ASSERT3(X,M,L) STATIC_ASSERT4(X, M, L)
#define STATIC_ASSERT2(X,M,L) STATIC_ASSERT3(X,M,L)
#define STATIC_ASSERT(X,M)    STATIC_ASSERT2(X,M,__LINE__)
#else
#define SA2TXT2(x) # x
#define SA2TXT(x) SA2TXT2(x)
#define STATIC_ASSERT(X,M) static_assert(X, SA2TXT(M))
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief typed pixel structures for every LED protocol supported.
/// These simplify access to a pixel byte array, or even to send typed pixels
/// to the LED strip, as the programmer directly access pixel colors by name.
////////////////////////////////////////////////////////////////////////////////

// Pixel colors byte order, 0 and 7 invalid.
#define PT_RGB   0b00100000
#define PT_GRB   0b01000000
#define PT_BGR   0b10000000
//#define PT_RBG 0b11000000
//#define PT_GBR 0b10100000
//#define PT_BRG 0b01100000
#define PT_COL   0b11100000 // Mask for 3-byte colors
#define PT_IS_SAME_COLOR(v1, v2) ((v1) & T_COL == (v2) & T_COL)

// Extra pixels bytes, 0 none, 2 invalid.
#define PT_XXXW  0b00000001 // Postfix white pixel brightness (sk6812)
#define PT_BXXX  0b00000010 // APA 102 prefix brightness pixel 0b111bbbbb
#define PT_XBYT  0b00000011 // Mask for extra byte(s)
#define PT_HAS_WHITE(v)  ((v) & PT_XBYT == PT_XXXW)
#define PT_HAS_BRIGHT(v) ((v) & PT_XBYT == PT_BXXX)
#define PT_BYTES_PER_PIXEL(v) (((v) & PT_XBYT != 0) ? 4 : 3)


// apa102, apa106 native color order
typedef struct {
	static const uint8_t type = PT_RGB;
	union { uint8_t r; uint8_t red; };
	union { uint8_t g; uint8_t green; };
	union { uint8_t b; uint8_t blue; };
} rgb;

// apa104, ws2812 native color order
typedef struct {
	static const uint8_t type = PT_GRB;
	union { uint8_t g; uint8_t green; };
	union { uint8_t r; uint8_t red; };
	union { uint8_t b; uint8_t blue; };
} grb;

typedef struct {
	static const uint8_t type = PT_BGR;
	union { uint8_t b; uint8_t blue; };
	union { uint8_t g; uint8_t green; };
	union { uint8_t r; uint8_t red; };
} bgr;

// sk6812 native color order
typedef struct {
	static const uint8_t type = PT_RGB | PT_XXXW;
	union { uint8_t r; uint8_t red; };
	union { uint8_t g; uint8_t green; };
	union { uint8_t b; uint8_t blue; };
	union { uint8_t w; uint8_t white; };
} rgbw;

// sk6812 native color order
typedef struct {
	static const uint8_t type = PT_GRB | PT_XXXW;
	union { uint8_t g; uint8_t green; };
	union { uint8_t r; uint8_t red; };
	union { uint8_t b; uint8_t blue; };
	union { uint8_t w; uint8_t white; };
} grbw;

// apa102 native color order
typedef struct {
	static const uint8_t type = PT_BGR | PT_BXXX;
	union { uint8_t B; uint8_t brightness;
	        uint8_t w; uint8_t white;
	        uint8_t h; uint8_t header; };
	union { uint8_t b; uint8_t blue; };
	union { uint8_t g; uint8_t green; };
	union { uint8_t r; uint8_t red; };
} hbgr;

/// @brief This macro lists all the LED structure types suported by the API
#define API_LIST        \
	API_ENTRY(rgb)  \
	API_ENTRY(grb)  \
	API_ENTRY(bgr)  \
	API_ENTRY(rgbw) \
	API_ENTRY(grbw) \
	API_ENTRY(hbgr)

////////////////////////////////////////////////////////////////////////////////
/// @brief Helper macro for palette index encoding into a char * array when
/// using an 8bit or less per pixel with a uint8_t type.
////////////////////////////////////////////////////////////////////////////////

/// @brief computes the size of a uint8_t array that uses pixels encoded with a
/// palette. For example: uint8_t buffer[ARRAY_SIZE(128, 2)]; is an array of 128
/// pixels encoded with 2 bits per pixel (4 colors).
#define ARRAY_SIZE(numPixels, bitsPerPixel) (((numPixels)+7) / 8 * (bitsPerPixel))

/// @brief Encode a pixel's color
/// For example, for encoding colors on 2 bits (4 colors, 0,1,2,3, in the palette),
/// Encoding color #3, using 2 bits per pixels, in the buffer for pixel i:
/// SET_PIXEL(buffer, i, 2, 3);
#define SET_PIXEL(array, index, bitsPerPixel, color)                           \
	_SET_PIXEL((array), (index), (bitsPerPixel), (color))

/// @brief extract a pixel's palette color index from a pixel array
/// For example, colorIndex = GET_PIXEL(buffer, i, 2);
#define GET_PIXEL(array, index, bitsPerPixel)                                  \
	_GET_PIXEL((array), (index), (bitsPerPixel))

// Internal definitions
#define _SET_PIXEL(array, index, bitsPerPixel, color)                          \
		array[index/(8/bitsPerPixel)] = (                              \
			array[index/(8/bitsPerPixel)] &                        \
			~(((1<<bitsPerPixel)-1) << ((index * bitsPerPixel)%8)) \
		) | color << ((index * bitsPerPixel)%8)


#define _GET_PIXEL(array, index, bitsPerPixel) (                               \
			array[index/(8/bitsPerPixel)] >>(index*bitsPerPixel)%8 \
		) & ((1<<bitsPerPixel)-1)


////////////////////////////////////////////////////////////////////////////////
/// @brief Conversion between cycles and nano seconds
////////////////////////////////////////////////////////////////////////////////
#define NS_PER_SEC          1000000000ULL
#define CYCLES_PER_SEC      ((uint64_t) (F_CPU))
#define CYCLES(time_ns)     (((CYCLES_PER_SEC * (time_ns)) + NS_PER_SEC - 1ULL) / NS_PER_SEC)
#define NANOSECONDS(cycles) (((cycles) * NS_PER_SEC + CYCLES_PER_SEC-1) / CYCLES_PER_SEC)


////////////////////////////////////////////////////////////////////////////////
/// @brief definitions for class template specializations
////////////////////////////////////////////////////////////////////////////////

/// @brief AVR ports are referenced A through F. For example ws2812b<D,6>
enum avrLedStripPort {
	A = 1,
	B = 2,
	C = 3,
	D = 4,
	E = 5,
	F = 6
};

/// @brief Type of low-level method to send data for the LED strip (see sendBytes)
enum ledProtocol {
	ONE_PORT_BITBANG = 1,   // Any LED with single data line
	TWO_PORT_SPLIT_BITBANG = 2, // Same, but update 2 ports in parallel, sending 1/2 the array to one port, and the other 1/2 to the other
	TWO_PORT_INTLV_BITBANG = 3, // Same, but update 2 ports in parallel, interleaving the pixels of the array
	EIGHT_PORT_BITBANG = 4, // Experimental
	ONE_PORT_PWM = 5,       // Not implemented
	ONE_PORT_UART = 6,      // Not implemented

	SPI_BITBANG = 7,        // APA-102 and any LED with data and clock line
	SPI_HARDWARE = 8        // Not implemented
};
#define PROTOCOL_SPI SPI_BITBANG


////////////////////////////////////////////////////////////////////////////////
/// @brief Hack to survive undefined port on Arduino
/// avoid compilation errors for arduinos that are missing some of the 4 ports
/// by defining all the unknown ports to be any of the known ones. This quiets
/// the compiler, and that fluff code optimizes away.
////////////////////////////////////////////////////////////////////////////////

// Define a DUMMY_PORT_ID that will be used to patch unknown ports to map to
// the first existing port we find. Undefined ports won't be used but this
// lets the compiler work without complaining.
#if defined(PORTD)
#define DUMMY_PORT PORTD
#define DUMMY_DDR   DDRD
#elif defined(PORTB)
#define DUMMY_PORT PORTB
#define DUMMY_DDR   DDRB
#elif defined(PORTA)
#define DUMMY_PORT PORTA
#define DUMMY_DDR   DDRA
#elif defined(PORTC)
#define DUMMY_PORT PORTC
#define DUMMY_DDR   DDRC
#endif // PORT

// If any of the ports we support does not exist, re-map it to the dummy port.
#ifdef DUMMY_PORT
#ifndef PORTA
#define PORTA DUMMY_PORT
#define  DDRA DUMMY_DDR
#endif // PORTA
#ifndef PORTB
#define PORTB DUMMY_PORT
#define  DDRB DUMMY_DDR
#endif // PORTB
#ifndef PORTC
#define PORTC DUMMY_PORT
#define  DDRC DUMMY_DDR
#endif // PORTC
#ifndef PORTD
#define PORTD DUMMY_PORT
#define  DDRD DUMMY_DDR
#endif // PORTD
#ifndef PORTE
#define PORTE DUMMY_PORT
#define  DDRE DUMMY_DDR
#endif // PORTE
#ifndef PORTF
#define PORTF DUMMY_PORT
#define  DDRF DUMMY_DDR
#endif // PORTF
#endif // DUMMY_PORT


////////////////////////////////////////////////////////////////////////////////
#ifdef ARDUINO_ARCH_AVR
////////////////////////////////////////////////////////////////////////////////
/// @brief Arduino AVR low level macros
////////////////////////////////////////////////////////////////////////////////

/// Port Data Direction control Register address
#define AVR_DDR(id) _AVR_DDR((id))
#define _AVR_DDR(id) ((id==A) ? DDRA : (id==B) ? DDRB : (id==C) ? DDRC : \
		(id==D) ? DDRD : (id==E) ? DDRE : DDRF)
#define SET_DDR_HIGH( portId, portPin) AVR_DDR(portId)  |= 1U << portPin
#define FAB_DDR(portId, val) AVR_DDR(portId) = val

/// Port address & pin level manipulation
#define AVR_PORT(id) _AVR_PORT((id))
#define _AVR_PORT(id) ((id==A) ? PORTA : (id==B) ? PORTB : (id==C) ? PORTC : \
		(id==D) ? PORTD : (id==E) ? PORTE : PORTF)
#define FAB_PORT(portId, val) AVR_PORT(portId) = val
// Note: gcc converts these bit manipulations to sbi and cbi instructions
#define SET_PORT_HIGH(portId, portPin) AVR_PORT(portId) |= 1U << portPin
#define SET_PORT_LOW( portId, portPin) AVR_PORT(portId) &= ~(1U << portPin);

/// Method to optimally delay N cycles with nops for bitBang.
#define DELAY_CYCLES(count) if (count > 0) __builtin_avr_delay_cycles(count);

// Number of cycles sbi and cbi instructions take when using SET macros
const int sbiCycles = 2;
const int cbiCycles = 2;


////////////////////////////////////////////////////////////////////////////////
#elif defined(__arm__)
////////////////////////////////////////////////////////////////////////////////
/// @brief ARM0 - ARM3 low level macros
/// @note: Ports have no letter, just port pins. ws2812b<D,6> will ignore D and
/// just route to pin 6.
///
/// Register information:
/// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0337h/BABJFFGJ.html
/// DWT_CYCCNT: Cycle count register
////////////////////////////////////////////////////////////////////////////////

/// @todo THE debug() function template resolution does not work on non optimized teensy
//#define DISABLE_DEBUG_METHOD

#define SET_DDR_HIGH( portId, portPin)
#define FAB_DDR( portId, val)
#define FAB_PORT(portId, val)

#define SET_PORT_HIGH(portId, pinId)   digitalWriteFast(pinId, 1)
#define SET_PORT_LOW( portId, pinId)   digitalWriteFast(pinId, 0)

/// Delay N cycles using cycles register
#define DELAY_CYCLES(count) {int till = count + ARM_DWT_CYCCNT; while (ARM_DWT_CYCCNT < till);}

// Number of cycles sbi and cbi instructions take
const int sbiCycles = 2;
const int cbiCycles = 2;

//mov r0, #COUNT
//L:
//subs r0, r0, #1
//bnz L
#define MACRO_CMB( A , B)           A##B
#define M_RPT(__N, __macro)         MACRO_CMB(M_RPT, __N)(__macro)
#define M_RPT0(__macro)
#define M_RPT1(__macro)             M_RPT0(__macro)   __macro(0)
#define M_RPT2(__macro)             M_RPT1(__macro)   __macro(1)
//...
#define MY_NOP(__N)                 __asm ("nop");
#define delay150cycles M_RPT(150, MY_NOP);

////////////////////////////////////////////////////////////////////////////////
#elif defined(ESP8266)

#error "Unsupported Tensilica (ARC)Architecture"

#else
////////////////////////////////////////////////////////////////////////////////
/// @brief unknown processor architecture
////////////////////////////////////////////////////////////////////////////////

#error "Unsupported Architecture"

////////////////////////////////////////////////////////////////////////////////
#endif // CPU ARCHITECTURE
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
// Base class defining LED strip operations allowed.
////////////////////////////////////////////////////////////////////////////////
static const uint8_t blank[3] = {128,128,128};

#define FAB_TDEF int16_t high1,             \
		int16_t low1,               \
		int16_t high0,              \
		int16_t low0,               \
		uint32_t minMsRefresh,      \
		avrLedStripPort dataPortId, \
		uint8_t dataPortPin,        \
		avrLedStripPort clockPortId,\
		uint8_t clockPortPin,       \
		class pixelClass,         \
		ledProtocol protocol

#define FAB_TVAR high1, low1, high0, low0, minMsRefresh, dataPortId, dataPortPin, clockPortId, clockPortPin, pixelClass, protocol

/// @brief Class to drive LED strips. Relies on custom sendBytes() method to push data to LEDs
//template<
//	int16_t high1,          // Number of cycles high for logical one
//	int16_t low1,           // Number of cycles  low for logical one
//	int16_t high0,          // Number of cycles high for logical zero
//	int16_t low0,           // Number of cycles  low for logical zero
//	uint32_t minMsRefresh,  // Minimum milliseconds to wait to reset LED strip data stream
//	avrLedStripPort dataPortId, // AVR port the LED strip is attached to
//	uint8_t dataPortPin         // AVR port bit the LED strip is attached to
//>
template <FAB_TDEF>
class avrBitbangLedStrip
{
	/// @brief Constant declaring if LED strip uses 3 or 4 bytes per pixel
	static const uint8_t stripBPP = PT_BYTES_PER_PIXEL(pixelClass::type);
	/// @brief Counts how many pixels were sent on APA102 strip before refresh
	static uint16_t pixelsDisplayed;
	static uint8_t oldSREG;

	typedef struct {
		static const uint8_t type = 0;
		uint8_t f0;
		uint8_t f1;
		uint8_t f2;
		uint8_t f3;
	} anon_pixel;

	public:
	////////////////////////////////////////////////////////////////////////
	/// @brief Constructor: Set selected dataPortId.dataPortPin to digital output
	////////////////////////////////////////////////////////////////////////
	avrBitbangLedStrip();
	////////////////////////////////////////////////////////////////////////
	~avrBitbangLedStrip() { };

	////////////////////////////////////////////////////////////////////////
	/// @brief Prints to console the configuration
	/// @note You must implement the template print routines (see example)
	/// Method can be disabled with:
	/// #define DISABLE_DEBUG_METHOD
	////////////////////////////////////////////////////////////////////////
	template <void printChar(const char *),void printInt(uint32_t)>
	static inline void debug(void);

	////////////////////////////////////////////////////////////////////////
	/// @brief Sends N bytes to the LED using bit-banging.
	///
	/// @param[in] count Number of bytes sent
	/// @param[in] array Array of bytes to send
	///
	/// @warning
	/// The caller must handle interupts!
	/// If interupts are not off, the timer interupt will happen, and it takes
	/// too long and will cause a LED strip reset.
	///
	/// @note
	/// __attribute__ ((always_inline)) forces gcc to inline this function in
	/// the caller, to optimize there and avoid function call overhead. The
	/// timing is critical at 16MHz.
	/// @note
	/// __builtin_avr_delay_cycles() is a gcc built-in that will generate ASM
	/// to implement the exact number of cycle delays specified using the most
	/// compact instruction loop, and is portable.
	/// @note
	/// The AVR port bit-set math will be changed by gcc to a sbi/cbi in ASM
	////////////////////////////////////////////////////////////////////////
	static inline void
	sendBytes(const uint16_t count, const uint8_t * array)
	__attribute__ ((always_inline));

	////////////////////////////////////////////////////////////////////////
	/// @brief Sends N uint32_t in a row set to zero or 0xFFFFFFFF, to build
	/// a frame for each pixel, and for a whole strip, SPI protocol only
	////////////////////////////////////////////////////////////////////////
	static inline void
	spiSoftwareSendFrame(const uint16_t count, bool high)
	__attribute__ ((always_inline));

	////////////////////////////////////////////////////////////////////////
	/// @bried Implements sendBytes for the 1-port SPI protocol
	////////////////////////////////////////////////////////////////////////
	static inline void
	spiSoftwareSendBytes(const uint16_t count, const uint8_t * array)
	__attribute__ ((always_inline));

	////////////////////////////////////////////////////////////////////////
	/// @brief Implements sendBytes for the 1-ports protocol
	////////////////////////////////////////////////////////////////////////
	static inline void
	onePortSoftwareSendBytes(const uint16_t count, const uint8_t * array)
	__attribute__ ((always_inline));

	////////////////////////////////////////////////////////////////////////
	/// @brief Implements sendBytes for the 2-ports protocol
	////////////////////////////////////////////////////////////////////////
	static inline void
	twoPortSoftwareSendBytes(const uint16_t count, const uint8_t * array)
	__attribute__ ((always_inline));

	////////////////////////////////////////////////////////////////////////
	/// @brief Implements sendBytes for the 8-ports protocol
	////////////////////////////////////////////////////////////////////////
	static inline void
	eightPortSoftwareSendBytes(const uint16_t count, const uint8_t * array)
	__attribute__ ((always_inline));





	////////////////////////////////////////////////////////////////////////
	/// @brief Clears the LED strip
	/// @param[in] count  Number of pixels to erase
	////////////////////////////////////////////////////////////////////////
	static inline void clear(const uint16_t count) __attribute__ ((always_inline));

	////////////////////////////////////////////////////////////////////////
	/// @brief Sets the LED strip to a grey value
	/// @param[in] count  Number of pixels to erase
	/// @param[in] value  Brightness of the grey [0..255]
	////////////////////////////////////////////////////////////////////////
	static inline void grey(
			const uint16_t count,
			const uint8_t value)
	__attribute__ ((always_inline));


	////////////////////////////////////////////////////////////////////////
	/// @brief Starts a write sequence to the LED strip
	/// @note: Call end() as soon as possible
	////////////////////////////////////////////////////////////////////////
	static inline void
	begin(void) __attribute__ ((always_inline));

	////////////////////////////////////////////////////////////////////////
	/// @brief Ends a write sequence to the LED strip
	/// @note: Call end() as soon as possible after begin()
	////////////////////////////////////////////////////////////////////////
	static inline void
	end(void) __attribute__ ((always_inline));


	////////////////////////////////////////////////////////////////////////
	/// @brief Display the array of pixels at the current position in the
	/// LED strip.
	/// @param[in] count  Number of pixels to send
	/// @param[in] array  Pixel array of size equal or greater than count.
	////////////////////////////////////////////////////////////////////////
	private:
	template <class pixelClassF>
	static inline void
	send(const uint16_t count, const pixelClassF * array)
	__attribute__ ((always_inline));

	template <class pixelClassF>
	static inline void
	send(const uint16_t count, const pixelClassF * palette, const uint8_t * array)
	__attribute__ ((always_inline));

	template <const uint8_t bitsPerPixel, class arrayClassF, class paletteClassF, class mapIntF>
	static inline void
	send(const uint16_t count, const arrayClassF * array, const paletteClassF * palette, const mapIntF * map = NULL)
	__attribute__ ((always_inline));


	public:
	////////////////////////////////////////////////////////////////////////
	/// @brief
	/// send(uint16_t count, pixelType * array);
	/// send(uint16_t count, uint16_t * array, brightness);
	///
	/// Display the array of pixels at the current position in the LED strip
	///
	/// @param[in] count  Number of pixels to send
	/// @param[in] array  Pixel array of 16bit/pixel {r:5, g:6, b:5}
	/// @param[in] brightness (optional) value 0..7 increases LED power
	///
	/// @note
	/// uint16_t array (assume r:5,g:6,b:5)
	/// uint8_t array (assume bytes are in native LED strip format)
	/// For 32bit, assume hbgr. The first byte must be > 0xE0 if the strip
	/// is APA-102!
	////////////////////////////////////////////////////////////////////////
	static inline void
	send(const uint16_t count, const uint16_t * array, const uint8_t brightness=1)
	__attribute__ ((always_inline));

	static inline void send(const uint16_t count, const uint8_t * array) {
		send<pixelClass>(count/stripBPP, (pixelClass *) array);
	} __attribute__ ((always_inline));

	static inline void send(const uint16_t count, const uint32_t * array) {
		send<hbgr>(count, (hbgr *) array);
	} __attribute__ ((always_inline));

	// Handle all pixel types: The API function is a wrapper around the template
	// send<type>, which specifies explicitly which pixel type is allowed. The
	// template function called will automatically translate to a native call
	// if the input type matches the LED strip type, else it will convert it.
#define API_ENTRY(entryPixelType) \
	static inline void send(const uint16_t count, const entryPixelType * array) { \
		send<entryPixelType>(count, array); } __attribute__ ((always_inline));
	API_LIST;
#undef API_ENTRY


	////////////////////////////////////////////////////////////////////////
	/// @brief
	/// send(uint16_t count, pixelType * array, uint8_t * map);
	/// send(uint16_t count, pixelType * array, uint16_t * map);
	/// send(uint16_t count, uint16_t * array, uint8_t * map, brightness);
	/// send(uint16_t count, uint16_t * array, uint16_t * map, brightness);
	///
	/// Remapping:
	/// Display the array of pixels at the current position in the LED strip
	/// The pixels are remapped. The map[] tells the offset in the array[]
	/// to read the color from. map[] entries correspond to the physical LED
	/// position in the strip. The array[] entries correspond to the logical
	/// pixels in the picture.
	///
	/// @param[in] count  Number of pixels to send
	/// @param[in] array  Pixel array of 16bit/pixel {r:5, g:6, b:5}
	/// @param[in] map    Array to remap pixels,
	/// @param[in] brightness (optional) value 0..7 increases LED power
	////////////////////////////////////////////////////////////////////////

	// 8bit map
//	static inline void
//	send(const uint16_t count, const uint16_t * array, uint8_t * map, const uint8_t brightness = 1)
//	__attribute__ ((always_inline));
	static inline void send(const uint16_t count, const uint8_t * array, uint8_t * map) {
		send<0,pixelClass,pixelClass,uint8_t>(count/stripBPP, (pixelClass *) array, NULL, map);
	} __attribute__ ((always_inline));
	static inline void send(const uint16_t count, const uint32_t * array, uint8_t * map) {
		send<0,hbgr,hbgr,uint8_t>(count, (hbgr *) array, NULL, map);
	} __attribute__ ((always_inline));
#define API_ENTRY(entryPixelType) \
	static inline void send(const uint16_t count, const entryPixelType * array, uint8_t * map) { \
		send<255, entryPixelType, entryPixelType, uint8_t>(count, array, NULL, map); \
	} __attribute__ ((always_inline));
	API_LIST

	// 16bit map
//	static inline void
//	send(const uint16_t count, const uint16_t * array, uint16_t * map, const uint8_t brightness = 1)
//	__attribute__ ((always_inline));
	static inline void send(const uint16_t count, const uint8_t * array, uint16_t * map) {
		send<0,pixelClass,pixelClass,uint16_t>(count/stripBPP, (pixelClass *) array, NULL, map);
	} __attribute__ ((always_inline));
	static inline void send(const uint16_t count, const uint32_t * array, uint16_t * map) {
		send<0,hbgr,hbgr,uint16_t>(count, (hbgr *) array, NULL, map);
	} __attribute__ ((always_inline));
#define API_ENTRY(entryPixelType) \
	static inline void send(const uint16_t count, const entryPixelType * array, uint16_t * map) { \
		send<0,entryPixelType,entryPixelType,uint16_t>(count, array, NULL, map); \
	} __attribute__ ((always_inline));
	API_LIST
#undef API_ENTRY

	////////////////////////////////////////////////////////////////////////
	/// @brief
	/// send<N>(uint16_t count, uint8_t array, pixelType * palette, uint8_t map);
	/// send<N>(uint16_t count, uint16_t array, pixelType * palette, uint16_t map);
	/// send<N>(uint16_t count, uint8_t array, uint16_t * palette, uint8_t map, bright);
	/// send<N>(uint16_t count, uint16_t array, uint16_t * palette, uint16_t map, bright);
	///
	/// Palette:
	/// Display the array of pixels at the current position in the LED strip
	/// The pixels use a color palette. The array[] holds N-bits for each
	/// pixel, the value represents a color index in the palette table.
	/// The 
	/// to read the color from. map[] entries correspond to the physical LED
	/// position in the strip. The array[] entries correspond to the logical
	/// pixels in the picture.
	///
	/// @param[in] N      Template parameter, number of bits per pixel.
	///                   Valid values: 1, 2, 4 or 8 bits per pixel.
	/// @param[in] count  Number of pixels to send
	/// @param[in] array  Pixel array of N bits per pixel
	/// @param[in] map    (optional) Remapping. map[LEDindex] = pixelOffset
	/// @param[in] bright (optional) value 0..7 increases LED power
	////////////////////////////////////////////////////////////////////////


//	template <const uint8_t bitsPerPixel>
//	static inline void send(const uint16_t count, const uint8_t * array, uint16_t * palette, uint8_t map = NULL, const uint8_t brightness = 1);
	template <const uint8_t bitsPerPixel>
	static inline void send(const uint16_t count, const uint8_t * array, uint8_t * palette, uint8_t map = NULL) {
		send<bitsPerPixel,uint8_t,pixelClass,uint8_t>(count/stripBPP, array, (pixelClass *) palette, map);
	} __attribute__ ((always_inline));
	template <const uint8_t bitsPerPixel>
	static inline void send(const uint16_t count, const uint8_t * array, uint32_t * palette, uint8_t map = NULL) {
		send<bitsPerPixel,uint8_t,hbgr,uint8_t>(count, array, (hbgr *) palette, map);
	} __attribute__ ((always_inline));
#define API_ENTRY(entryPixelType) \
	template <const uint8_t bitsPerPixel> \
	static inline void send(const uint16_t count, const uint8_t * array, const entryPixelType * palette, uint8_t map = NULL) { \
		send<bitsPerPixel, uint8_t, entryPixelType, uint8_t>(count, array, palette, map); \
	} __attribute__ ((always_inline));
	API_LIST
#undef API_ENTRY

//	template <const uint8_t bitsPerPixel>
//	static inline void send(const uint16_t count, const uint16_t * array, uint16_t * palette, uint16_t map = NULL, const uint8_t brightness = 1);
	template <const uint8_t bitsPerPixel>
	static inline void send(const uint16_t count, const uint16_t * array, uint8_t * palette, uint16_t map = NULL) {
		send<bitsPerPixel,uint16_t,pixelClass,uint16_t>(count/stripBPP, array, (pixelClass *) palette, map);
	} __attribute__ ((always_inline));
	template <const uint8_t bitsPerPixel>
	static inline void send(const uint16_t count, const uint16_t * array, uint32_t * palette, uint16_t map = NULL) {
		send<bitsPerPixel,uint16_t,hbgr,uint16_t>(count, array, (hbgr *) palette, map);
	} __attribute__ ((always_inline));
#define API_ENTRY(entryPixelType) \
	template <const uint8_t bitsPerPixel> \
	static inline void send(const uint16_t count, const uint16_t * array, const entryPixelType * palette, uint16_t map = NULL) { \
		send<bitsPerPixel, uint16_t, entryPixelType, uint16_t>(count, array, palette, map); \
	} __attribute__ ((always_inline));
	API_LIST
#undef API_ENTRY




	////////////////////////////////////////////////////////////////////////
	/// @brief Display the array of pixels at the current position in the
	/// LED strip.
	/// @param[in] count  Number of pixels to send
	/// @param[in] array  Pixel array of size equal or greater than count.
	////////////////////////////////////////////////////////////////////////
	private:
	template <class pixelClassF>
	static inline void
	draw(const uint16_t count, const pixelClassF * array) {
		begin();
		send(count, array);
		end();
	} __attribute__ ((always_inline));

	public:
	////////////////////////////////////////////////////////////////////////
	/// @brief Display the array of pixels at the current position in the
	/// LED strip.
	/// @param[in] count  Number of pixels to send
	/// @param[in] array  Pixel array of 16bit/pixel {r:5, g:6, b:5}
	/// @param[in] brightness (optional) value 0..7 increases LED power
	////////////////////////////////////////////////////////////////////////
	static inline void
	draw(const uint16_t count, const uint16_t * array, const uint8_t brightness = 1)
	__attribute__ ((always_inline));
	// For 8bit, assume bytes are laid out same as native strip format.
	static inline void draw(const uint16_t count, const uint8_t * array) {
		draw<pixelClass>(count/stripBPP, (pixelClass *) array); } __attribute__ ((always_inline));
	// For 32bit, assume hbgr. The first byte must be > 0xE0 if the strip is APA-102!
	static inline void draw(const uint16_t count, const uint32_t * array) {
		draw<hbgr>(count, (hbgr *) array); } __attribute__ ((always_inline));
	// Handle all other valid pixel types: The API function is a wrapper around the template
	// draw<type>, which specifies explicitly which type is allowed. The template will automatically
	// translate to a native call if the input type matches the LED strip type else it will convert it.
	static inline void draw(const uint16_t count, const rgb * array) {
		draw<rgb>(count, array); } __attribute__ ((always_inline));
	static inline void draw(const uint16_t count, const grb * array) {
		draw<grb>(count, array); } __attribute__ ((always_inline));
	static inline void draw(const uint16_t count, const bgr * array) {
		draw<bgr>(count, array); } __attribute__ ((always_inline));
	static inline void draw(const uint16_t count, const rgbw * array) {
		draw<rgbw>(count, array); } __attribute__ ((always_inline));
	static inline void draw(const uint16_t count, const grbw * array) {
		draw<grbw>(count, array); } __attribute__ ((always_inline));
	static inline void draw(const uint16_t count, const hbgr * array) {
		draw<hbgr>(count, array); } __attribute__ ((always_inline));
};


////////////////////////////////////////////////////////////////////////////////
/// Class methods definition
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Constructor
////////////////////////////////////////////////////////////////////////////////
template<FAB_TDEF>
uint16_t avrBitbangLedStrip<FAB_TVAR>::pixelsDisplayed = 0;

template<FAB_TDEF>
uint8_t avrBitbangLedStrip<FAB_TVAR>::oldSREG = SREG;

template<FAB_TDEF>
avrBitbangLedStrip<FAB_TVAR>::avrBitbangLedStrip()
{

	// Digital out pin mode
	// bitSet(portDDR, dataPortPin);
	// DDR? |= 1U << dataPortPin;
	switch (protocol) {
		case TWO_PORT_SPLIT_BITBANG:
		case TWO_PORT_INTLV_BITBANG:
			// Init two ports as out, set to low state
			SET_DDR_HIGH(dataPortId,  dataPortPin);
			SET_DDR_HIGH(clockPortId, clockPortPin);
			SET_PORT_LOW(dataPortId,  dataPortPin);
			SET_PORT_LOW(clockPortId, clockPortPin);
			break;
		case EIGHT_PORT_BITBANG:
			FAB_DDR(dataPortId, 0xFF); // all pins out
			FAB_PORT(dataPortId, 0x00); // all pins low
			break;
		case SPI_BITBANG:
			// Init both ports as out
			SET_DDR_HIGH(dataPortId, dataPortPin);
			SET_DDR_HIGH(clockPortId, clockPortPin);
			// SPI: Reset LED strip to accept a refresh
			spiSoftwareSendFrame(16, false);
			break;
		default:
			// Init data port as out, set to low state
			SET_DDR_HIGH(dataPortId, dataPortPin);
			SET_PORT_LOW(dataPortId, dataPortPin);
			break;
	}
};


////////////////////////////////////////////////////////////////////////////////
// Debug routine
////////////////////////////////////////////////////////////////////////////////
template<FAB_TDEF>
template <void printChar(const char *),void printInt(uint32_t)>
inline void
avrBitbangLedStrip<FAB_TVAR>::debug(void)
{
#ifndef DISABLE_DEBUG_METHOD
	printChar("\nclass avrBitbangLedStrip<...>\n");

	printInt(CYCLES_PER_SEC/1000000);
	printChar("MHz CPU, ");
	printInt(NANOSECONDS(1000));
	printChar(" picoseconds per cycle\n");

	printChar("ONE  HIGH=");
	printInt(high1);
	printChar(" LOW=");
	printInt(low1);
	printChar(" cycles\n");

	printChar("ZERO HIGH=");
	printInt(high0);
	printChar(" LOW=");
	printInt(low0);
	printChar(" cycles\n");

	if (pixelClass::type & PT_BXXX) {
		printChar("W");
	}
	switch (pixelClass::type & PT_COL) {
		case PT_RGB: printChar("RGB"); break;
		case PT_GRB: printChar("GRB"); break;
		case PT_BGR: printChar("BGR"); break;
		default: printChar("[ERROR]"); break;
	}
	if (pixelClass::type & PT_XXXW) {
		printChar("W");
	}

	printChar(" REFRESH MSEC=");
	printInt(minMsRefresh);

	printChar("\nDATA_PORT ");
	switch(dataPortId) {
		case A:
			printChar("A.");
			break;
		case B:
			printChar("B.");
			break;
		case C:
			printChar("C.");
			break;
		case D:
			printChar("D.");
			break;
		case E:
			printChar("E.");
			break;
		case F:
			printChar("F.");
			break;
		default:
			printChar("UNKNOWN.");
	}
	printInt(dataPortPin);
	printChar(", ");

	if (protocol >= PROTOCOL_SPI) {
		printChar("CLOCK_PORT ");
		switch(clockPortId) {
			case A:
				printChar("A.");
				break;
			case B:
				printChar("B.");
				break;
			case C:
				printChar("C.");
				break;
			case D:
				printChar("D.");
				break;
			case E:
				printChar("E.");
				break;
			case F:
				printChar("F.");
				break;
			default:
				printChar("UNKNOWN.");
		}
		printInt(clockPortPin);
		printChar(", ");
	}

	switch(protocol) {
		case ONE_PORT_BITBANG:
			printChar("ONE-PORT (bitbang)");
			break;
		case TWO_PORT_SPLIT_BITBANG:
			printChar("TWO-PORT-SPLIT (bitbang)");
			break;
		case TWO_PORT_INTLV_BITBANG:
			printChar("TWO-PORT-INTERLEAVED (bitbang)");
			break;
		case EIGHT_PORT_BITBANG:
			printChar("HEIGHT-PORT (bitbang)");
			break;
		case ONE_PORT_PWM:
			printChar("ONE-PORT (PWM)");
			break;
		case ONE_PORT_UART:
			printChar("ONE-PORT (UART)");
			break;
		case SPI_BITBANG:
			printChar("SPI (bitbang)");
			break;
		case SPI_HARDWARE:
			printChar("SPI (hardware)");
			break;
		default:
			printChar("PROTOCOL UNKNOWN");
	}
	printChar("\n");
#endif
}


////////////////////////////////////////////////////////////////////////////////
// Low level sendBytes routines
////////////////////////////////////////////////////////////////////////////////
template<FAB_TDEF>
inline void
avrBitbangLedStrip<FAB_TVAR>::sendBytes(const uint16_t count, const uint8_t * array)
{
	switch (protocol) {
		case ONE_PORT_BITBANG:
			onePortSoftwareSendBytes(count, array);
			break;
		case TWO_PORT_SPLIT_BITBANG:
		case TWO_PORT_INTLV_BITBANG:
			// Note: the function will detect and handle modes I and S
			twoPortSoftwareSendBytes(count, array);
			break;
		case EIGHT_PORT_BITBANG:
			eightPortSoftwareSendBytes(count, array);
			break;
		case SPI_BITBANG:
			spiSoftwareSendBytes(count, array);
			break;
	}
}

template<FAB_TDEF>
inline void
avrBitbangLedStrip<FAB_TVAR>::spiSoftwareSendFrame(const uint16_t count, bool high)
{
	if (high) {
		SET_PORT_HIGH(dataPortId, dataPortPin);
	} else {
		SET_PORT_LOW(dataPortId, dataPortPin);
	}
	// for(uint32_t c = 0; c < 32 * count; c++) {
	for(uint32_t c = 0; c < 8 * count; c++) {
		SET_PORT_LOW(clockPortId, clockPortPin);
		SET_PORT_HIGH(clockPortId, clockPortPin);
	}
}

template<FAB_TDEF>
inline void
avrBitbangLedStrip<FAB_TVAR>::spiSoftwareSendBytes(const uint16_t count, const uint8_t * array)
{
	for(uint16_t cnt = 0; cnt < count; ++cnt) {
		const uint8_t val = array[cnt];
		// If LED strip is defined as 3 byte type (default) then hard code the first
		// byte to 0xFF, aka max brightness.
		// This hard-codes the APA-102 protocol here so it is a bit hacky.
		if ((stripBPP >= 4) && ((cnt % 3) == 0)) {
//			spiSoftwareSendFrame(1, true);
		}
		// To send a bit to SPI, set its value, then transtion clock low-high
		for(int8_t b=7; b>=0; b--) {
			const bool bit = (val>>b) & 0x1;
			SET_PORT_LOW(clockPortId, clockPortPin);
 			if (bit) {
				SET_PORT_HIGH(dataPortId, dataPortPin);
			} else {
				SET_PORT_LOW(dataPortId, dataPortPin);
			}
			SET_PORT_HIGH(clockPortId, clockPortPin);
		}

	}
}

/// @brief sends the array split across two ports each having half the LED strip to illuminate.
/// To achieve this, we repurpose the clock port used for SPI as a second data port.
/// We support two protocols:
/// TWO_PORT_SPLIT_BITBANG: The array is split into 2 halves sent each sent to one of the ports
/// TWO_PORT_INTLV_BITBANG: The array is interleaved and each pixel of 3 byte is sent to the next port
template<FAB_TDEF>
inline void
avrBitbangLedStrip<FAB_TVAR>::twoPortSoftwareSendBytes(const uint16_t count, const uint8_t * array)
{
	// If split mode, we send a block of half size
	const uint16_t blockSize = (protocol == TWO_PORT_SPLIT_BITBANG) ? count/2 : count;

	// Stride is two for interlacing to jump pixels going to the other port
	const uint8_t stride = (protocol == TWO_PORT_SPLIT_BITBANG) ? 1 : 2;
	const uint8_t increment = stride * stripBPP;

	// Loop to scan all pixels, potentially skipping every other pixel, or scanning 1/2 the pixels
	// based on the display protocol used.
	for(uint16_t pix = 0; pix < blockSize; pix += increment) {
		// Loop to send 3 or 4 bytes of a pixel to the same port
		for(uint16_t pos = pix; pos < pix+stripBPP; pos++) {
			for(int8_t bit = 7; bit >= 0; bit--) {
				const uint8_t mask = 1 << bit;

				volatile bool isbitDhigh = array[pos] & mask;
	
				volatile bool isbitChigh = (protocol == TWO_PORT_SPLIT_BITBANG) ?
					array[pos + blockSize] & mask : // split: pixel is blockSize away.
					array[pos + stripBPP] & mask;        // interleaved: pixel is next one.

				if (isbitDhigh) SET_PORT_HIGH(dataPortId, dataPortPin);
				if (isbitChigh) SET_PORT_HIGH(clockPortId, clockPortPin);

				if (!isbitDhigh) {
					SET_PORT_HIGH(dataPortId, dataPortPin);
					DELAY_CYCLES(high0 - sbiCycles);
					SET_PORT_LOW(dataPortId, dataPortPin);
				}
				if (!isbitChigh) {
					SET_PORT_HIGH(clockPortId, clockPortPin);
					DELAY_CYCLES(high0 - sbiCycles);
					SET_PORT_LOW(clockPortId, clockPortPin);
				}
				DELAY_CYCLES(high1 - 2*sbiCycles);
				SET_PORT_LOW(dataPortId, dataPortPin);
				SET_PORT_LOW(clockPortId, clockPortPin);
				DELAY_CYCLES(low1 - 2*cbiCycles);
			}
		}
	}
}

#if 0
template<FAB_TDEF>
inline void
avrBitbangLedStrip<FAB_TVAR>::twoPortSoftwareSendBytes(const uint16_t count, const uint8_t * array)
{
	const uint16_t blockSize = count/2;

	for(uint16_t pos = 0; pos < blockSize; pos++) {
		for(int8_t bit = 7; bit >= 0; bit--) {
			const uint8_t mask = 1 << bit;
			const bool isbitDhigh = array[0*blockSize + pos] & mask;
			const bool isbitChigh = array[1*blockSize + pos] & mask;

			if (isbitDhigh) SET_PORT_HIGH(dataPortId, dataPortPin);
			if (isbitChigh) SET_PORT_HIGH(clockPortId, clockPortPin);

			if (!isbitDhigh) {
				SET_PORT_HIGH(dataPortId, dataPortPin);
				//DELAY_CYCLES(high0 - sbiCycles);
				DELAY_CYCLES(0);
				SET_PORT_LOW(dataPortId, dataPortPin);
			}
			if (!isbitChigh) {
				SET_PORT_HIGH(clockPortId, clockPortPin);
				//DELAY_CYCLES(high0 - sbiCycles);
				DELAY_CYCLES(0);
				SET_PORT_LOW(clockPortId, clockPortPin);
			}
			DELAY_CYCLES(0);
			SET_PORT_LOW(dataPortId, dataPortPin);
			SET_PORT_LOW(clockPortId, clockPortPin);
			DELAY_CYCLES(0);
		}
	}
}
#endif


template<FAB_TDEF>
inline void
avrBitbangLedStrip<FAB_TVAR>::eightPortSoftwareSendBytes(const uint16_t count, const uint8_t * array)
{
	const uint16_t blockSize __asm__("r14") = count / (clockPortPin - dataPortPin + 1) / stripBPP * stripBPP;

	// Buffer one byte of each port in register
	uint8_t r0 = 0; // __asm__("r2") = 0;
	uint8_t r1 = 0; // __asm__("r3") = 0;
	uint8_t r2 = 0; // __asm__("r4") = 0;
	uint8_t r3 = 0; // __asm__("r5") = 0;
	uint8_t r4 = 0; // __asm__("r6") = 0;
	uint8_t r5 = 0; // __asm__("r7") = 0;
	uint8_t r6 = 0; // __asm__("r8") = 0;
	uint8_t r7 = 0; // __asm__("r9") = 0;

	// Since bits for each port are sent delayed by one cycle to
	// implement the memory access pipeline, we must leave the
	// ports not yet in use set to LOW level. Once the pipeline
	// is initialized fully onMask == 0xFF, and we always set it
	// to high to send a zero or a one.
	uint8_t onMask __asm__("r10") = 0;

	for(uint16_t c __asm__("r12") = 0; c < blockSize ; ++c) {
		for(int8_t b __asm__("r13") = 7; b >= 0; --b) {
			uint8_t bitmask __asm__("r10") = 0;

			// Load ONE byte per iteration
			// Skip end condition check to reduce CPU cycles.
			// It is expected that the LED strip won't have extra pixels.
			switch (b) {
				case 0:
					// By checking statically if rX is read from memory, we
					// avoid changing it, and the compiler will optimize out all
					// the code including bitmask setting. On 16MHz Uno, we can
					// only set bitmask for 6 ports before the strip resets.
					if (0 == dataPortPin) {
						onMask |= 1;
						r0 = array[c];
					}

					if (r0 & 0b10000000) bitmask |= 1;
					if (r1 & 0b01000000) bitmask |= 2;
					if (r2 & 0b00100000) bitmask |= 4;
					if (r3 & 0b00010000) bitmask |= 8;
					if (r4 & 0b00001000) bitmask |= 16;
					if (r5 & 0b00000100) bitmask |= 32;
					if (r6 & 0b00000010) bitmask |= 64;
					if (r7 & 0b00000001) bitmask |= 128;
					break;
				case 1:
					if ((1 >= dataPortPin) && (1 <= clockPortPin)) {
						onMask |= 2;
						r1 = array[c + (1-dataPortPin) * blockSize];
					}

					if (r0 & 0b00000001) bitmask |= 1;
					if (r1 & 0b10000000) bitmask |= 2;
					if (r2 & 0b01000000) bitmask |= 4;
					if (r3 & 0b00100000) bitmask |= 8;
					if (r4 & 0b00010000) bitmask |= 16;
					if (r5 & 0b00001000) bitmask |= 32;
					if (r6 & 0b00000100) bitmask |= 64;
					if (r7 & 0b00000010) bitmask |= 128;
					break;
				case 2:
					if ((2 >= dataPortPin) && (2 <= clockPortPin)) {
						onMask |= 4;
						r2 = array[c + (2-dataPortPin) * blockSize];
					}

					if (r0 & 0b00000010) bitmask |= 1;
					if (r1 & 0b00000001) bitmask |= 2;
					if (r2 & 0b10000000) bitmask |= 4;
					if (r3 & 0b01000000) bitmask |= 8;
					if (r4 & 0b00100000) bitmask |= 16;
					if (r5 & 0b00010000) bitmask |= 32;
					if (r6 & 0b00001000) bitmask |= 64;
					if (r7 & 0b00000100) bitmask |= 128;
					break;
				case 3:
					if ((3 >= dataPortPin) && (3 <= clockPortPin)) {
						onMask |= 8;
						r3 = array[c + (3-dataPortPin) * blockSize];
					}

					if (r0 & 0b00000100) bitmask |= 1;
					if (r1 & 0b00000010) bitmask |= 2;
					if (r2 & 0b00000001) bitmask |= 4;
					if (r3 & 0b10000000) bitmask |= 8;
					if (r4 & 0b01000000) bitmask |= 16;
					if (r5 & 0b00100000) bitmask |= 32;
					if (r6 & 0b00010000) bitmask |= 64;
					if (r7 & 0b00001000) bitmask |= 128;
					break;
				case 4:
					if ((4 >= dataPortPin) && (4 <= clockPortPin)) {
						onMask |= 16;
						r4 = array[c + (4-dataPortPin) * blockSize];
					}

					if (r0 & 0b00001000) bitmask |= 1;
					if (r1 & 0b00000100) bitmask |= 2;
					if (r2 & 0b00000010) bitmask |= 4;
					if (r3 & 0b00000001) bitmask |= 8;
					if (r4 & 0b10000000) bitmask |= 16;
					if (r5 & 0b01000000) bitmask |= 32;
					if (r6 & 0b00100000) bitmask |= 64;
					if (r7 & 0b00010000) bitmask |= 128;
					break;
				case 5:
					if ((5 >= dataPortPin) && (5 <= clockPortPin)) {
						onMask |= 32;
						r5 = array[c + (5-dataPortPin) * blockSize];
					}

					if (r0 & 0b00010000) bitmask |= 1;
					if (r1 & 0b00001000) bitmask |= 2;
					if (r2 & 0b00000100) bitmask |= 4;
					if (r3 & 0b00000010) bitmask |= 8;
					if (r4 & 0b00000001) bitmask |= 16;
					if (r5 & 0b10000000) bitmask |= 32;
					if (r6 & 0b01000000) bitmask |= 64;
					if (r7 & 0b00100000) bitmask |= 128;
					break;
				case 6:
					if ((6 >= dataPortPin) && (6 <= clockPortPin)) {
						onMask |= 64;
						r6 = array[c + (6-dataPortPin) * blockSize];
					}

					if (r0 & 0b00100000) bitmask |= 1;
					if (r1 & 0b00010000) bitmask |= 2;
					if (r2 & 0b00001000) bitmask |= 4;
					if (r3 & 0b00000100) bitmask |= 8;
					if (r4 & 0b00000010) bitmask |= 16;
					if (r5 & 0b00000001) bitmask |= 32;
					if (r6 & 0b10000000) bitmask |= 64;
					if (r7 & 0b01000000) bitmask |= 128;
					break;
				case 7:
					if ((7 >= dataPortPin) && (7 <= clockPortPin)) {
						onMask |= 128;
						r7 = array[c + (7-dataPortPin) * blockSize];
					}

					if (r0 & 0b01000000) bitmask |= 1;
					if (r1 & 0b00100000) bitmask |= 2;
					if (r2 & 0b00010000) bitmask |= 4;
					if (r3 & 0b00001000) bitmask |= 8;
					if (r4 & 0b00000100) bitmask |= 16;
					if (r5 & 0b00000010) bitmask |= 32;
					if (r6 & 0b00000001) bitmask |= 64;
					if (r7 & 0b10000000) bitmask |= 128;
					break;
				}

			// Set all HIGH, set LOW all zeros, set LOW zeros and ones.
			FAB_PORT(dataPortId, onMask);
			DELAY_CYCLES(high0 - sbiCycles);

			FAB_PORT(dataPortId, bitmask);
			DELAY_CYCLES( high1 - high0 + sbiCycles);

			FAB_PORT(dataPortId, 0x00);
			// Estimate overhead of math above to ~ 20 cycles
			DELAY_CYCLES(low0 - cbiCycles - 20);
		}
	}
}


template<FAB_TDEF>
inline void
avrBitbangLedStrip<FAB_TVAR>::onePortSoftwareSendBytes(const uint16_t count, const uint8_t * array)
{

	for(uint16_t c=0; c < count; c++) {
		const uint8_t val = array[c];
		for(int8_t b=7; b>=0; b--) {
			const bool bit = (val>>b) & 0x1;
 
 			if (bit) {
				// Send a ONE

				// HIGH with ASM sbi (2 words, 2 cycles)
				SET_PORT_HIGH(dataPortId, dataPortPin);
				// Wait exact number of cycles specified
				DELAY_CYCLES(high1 - sbiCycles);
				//  LOW with ASM cbi (2 words, 2 cycles)
				SET_PORT_LOW(dataPortId, dataPortPin);
				// Wait exact number of cycles specified
				DELAY_CYCLES(low1 - cbiCycles);
			} else {
				// Send a ZERO

				// HIGH with ASM sbi (2 words, 2 cycles)
				SET_PORT_HIGH(dataPortId, dataPortPin);
				// Wait exact number of cycles specified
				DELAY_CYCLES(high0 - sbiCycles);
				//  LOW with ASM cbi (2 words, 2 cycles)
				SET_PORT_LOW(dataPortId, dataPortPin);
				// Wait exact number of cycles specified
				DELAY_CYCLES(low0 - cbiCycles);
			}
		}
	}
}


////////////////////////////////////////////////////////////////////////////////
// Helper routines
////////////////////////////////////////////////////////////////////////////////
template<FAB_TDEF>
inline void
avrBitbangLedStrip<FAB_TVAR>::clear(const uint16_t numPixels)
{
	grey(numPixels, 0);
}

template<FAB_TDEF>
inline void
avrBitbangLedStrip<FAB_TVAR>::grey(const uint16_t numPixels, const uint8_t value)
{
 	begin();
	STATIC_ASSERT(stripBPP >= 3, too_many_bytes_per_pixel);
	STATIC_ASSERT(stripBPP <= 4, too_many_bytes_per_pixel);
	if (stripBPP == 3) {
		const uint8_t array[3] = {value, value, value};
		for(uint16_t i = 0; i < numPixels; i++) {
			send(stripBPP, array);
		}
	} else if (stripBPP == 4) {
		const uint8_t array[4] = {value, value, value, value};
		for(uint16_t i = 0; i < numPixels; i++) {
			send(stripBPP, array);
		}
	}
	end();
}


////////////////////////////////////////////////////////////////////////////////
// Begin/End
////////////////////////////////////////////////////////////////////////////////

template<FAB_TDEF>
inline void
avrBitbangLedStrip<FAB_TVAR>::begin(void)
{
	if (protocol == SPI_BITBANG) {
		// SPI: Send start frame of 32 bits set to zero to force refresh
		spiSoftwareSendFrame(4, false);
	} else {
		// 1-wire: Delay next pixels to cause a refresh
		delay(minMsRefresh);
 		oldSREG = SREG;
		// Disable interrupts.
		__builtin_avr_cli();
	}
}

template<FAB_TDEF>
inline void
avrBitbangLedStrip<FAB_TVAR>::end(void)
{
	if (protocol == SPI_BITBANG) {
		// Send end frame equal to 1/2 bit per pixel sent.
		spiSoftwareSendFrame(++pixelsDisplayed/2, true);
		pixelsDisplayed = 0;
	} else {
		// Restore interrupts.
 		SREG = oldSREG;
	}
}

template<FAB_TDEF>
template <class pixelClassF>
inline void
avrBitbangLedStrip<FAB_TVAR>::send(
		const uint16_t numPixels,
		const pixelClassF * array)
{
	if (pixelClassF::type == pixelClass::type) {
		// Native type, send as-is
		sendBytes(numPixels * stripBPP, (uint8_t *) array);
	} else {
		pixelClass p = {};
		anon_pixel * ap = (anon_pixel *) & p;

		if (pixelClass::type & PT_XXXW) {
			ap->f3 = 0x00;
		}
		if (pixelClass::type & PT_BXXX) {
			ap->f0 = 0xFF;
		}
		for (uint16_t i = 0; i < numPixels; i++) {
			anon_pixel * aa = (anon_pixel *) & array[i];
			p.r = array[i].r;
			p.g = array[i].g;
			p.b = array[i].b;
			if (pixelClass::type & pixelClassF::type & PT_XXXW) {
				ap->f3 = aa->f3;
			}
			if (pixelClass::type & pixelClassF::type & PT_BXXX) {
				ap->f0 = aa->f0;
			}
			// Pixel p is a native type, send as-is
			sendBytes(stripBPP, (uint8_t *) &p);
		}
	}
	if (protocol == SPI_BITBANG) {
		pixelsDisplayed += numPixels;
	}
}

template<FAB_TDEF>
inline void
avrBitbangLedStrip<FAB_TVAR>::send(
		const uint16_t numPixels,
		const uint16_t * array,
		const uint8_t brightness)
{
	pixelClass p;

	if (pixelClass::type && PT_BXXX) {
		p.b = 0xFF;
	}
	if (pixelClass::type && PT_XXXW) {
		p.w = 0;
	}
	if (brightness == 0) {
		brightness = 1;
	}
	for (uint16_t i = 0; i < numPixels; i++) {
		p.r = (array[i] >> 11) * brightness;
		p.g = ((array[i] >> 5) & 0b111111) * brightness;
		p.b = (array[i] & 0b11111) * brightness;
		// Pixel p is a native type, send as-is
		sendBytes(stripBPP, (uint8_t *) &p);
	}
}


// The remap is optional. When it is used the remap array represents the physical
// LEDs of the LED strip, and holds the index in array[] of the color.
template<FAB_TDEF>
template <const uint8_t bitsPerPixel, class arrayClassF, class paletteClassF, class mapIntF>
inline void
avrBitbangLedStrip<FAB_TVAR>::send (
		const uint16_t count,
		const arrayClassF * array,   // uint8_t if palette, else pixelClassF
		const paletteClassF * palette, // any custom pixelClassF, but not uint8_t
		const mapIntF * map)         // use uint8_t for <256 pixels, else uint16_t
{
	// Support only palettes with 2, 4, 16 or 256 colors
	STATIC_ASSERT( bitsPerPixel == 1 || bitsPerPixel == 2 ||
		bitsPerPixel == 4 || bitsPerPixel == 8,
		Unsupported_bits_per_pixel_palette);

	// paletteClassF must be a custom type so the size is known, not uint8_t.
	const uint8_t pixelSize = sizeof(paletteClassF);
	const mapIntF iMax = (const mapIntF) count;

	for (mapIntF i = 0; i < iMax; i++) {
		// Remapped index in array is index of the next pixel to push.
		// Pixels are pushed in order of the LED strip.
		const mapIntF ri = (map != NULL) ? map[i] : i;

		// Extract the N-bit color index via bitmasks
		if (palette != NULL) {
			const uint8_t ci = GET_PIXEL(array, ri, bitsPerPixel);
		}

		// Get the color/pixel to send. Without palette, the array is the pixel
		const paletteClassF * pixel =  (palette != NULL) ? &palette[i] : &array[ri];

		// Draw pixel by pixel
		send(1, pixel);
	}
}
















/************************************************************************************************************************************
template<FAB_TDEF>
inline void
avrBitbangLedStrip<FAB_TVAR>::refresh(void) {
	if (protocol == SPI_BITBANG) {
		// SPI: Finish current display by sending pix/2 extra clocks
		// Reset LED strip with a zero-filled 32bit start frame
		spiSoftwareSendFrame(++pixelsDisplayed/2, true);
		spiSoftwareSendFrame(4, false);
		pixelsDisplayed = 0;
	} else {
		// 1-wire: Wait long enough to cause a refresh
		delay(minMsRefresh);
	}
}


// 3B raw input array
template<FAB_TDEF>
inline void
avrBitbangLedStrip<FAB_TVAR>::sendPixels(
		const uint16_t numPixels,
		const uint8_t * array)
{
 	DISABLE_INTERRUPTS;
	sendBytes(numPixels * bytesPerPixel, array);
	RESTORE_INTERRUPTS;
}


// Since colors is a constant, the switch case will convert to 4 sendBytes max.
#define SEND_REMAPPED_PIXELS(numPixels, array, sendWhite)
#define sendWhiteMacro sendBytes(1, &array[i].w)
#define SEND_REMAPPED_PIXELS_4B(numPixels, array) SEND_REMAPPED_PIXELS(numPixels, array, sendWhiteMacro)
#define SEND_REMAPPED_PIXELS_3B(numPixels, array) SEND_REMAPPED_PIXELS(numPixels, array, )

// 4B struct input arrays
template<FAB_TDEF>
inline void
avrBitbangLedStrip<FAB_TVAR>::sendPixels(
		const uint16_t numPixels,
		const rgbw * array)
{
	if (colors == RGBW || colors == NONE) {
		sendPixels(numPixels, (const uint8_t *) array);
	} else if (colors == RGB) {
		// Output array is same order but 3B, send as 32bit, which will be converted
		sendPixels(numPixels, (const uint32_t *) array);
	} else {
		// Handle input array of different format than LED strip
		SEND_REMAPPED_PIXELS_4B(numPixels, array);
	}
}

template<FAB_TDEF>
inline void
avrBitbangLedStrip<FAB_TVAR>::sendPixels(
		const uint16_t numPixels,
		const grbw * array)
{
	if (colors == GRBW || colors == NONE) {
		sendPixels(numPixels, (const uint8_t *) array);
	} else if (colors == GRB) {
		sendPixels(numPixels, (const uint32_t *) array);
	} else {
		// Handle input array of different format than LED strip
		SEND_REMAPPED_PIXELS_4B(numPixels, array);
	}
}

template<FAB_TDEF>
inline void
avrBitbangLedStrip<FAB_TVAR>::sendPixels(
		const uint16_t numPixels,
		const hbgr * array)
{
	if (colors == HBGR || colors == NONE) {
		sendPixels(numPixels, (const uint8_t *) array);
	} else if (colors == BGR) {
		sendPixels(numPixels, (const uint32_t *) array);
	} else {
		// Handle input array of different format than LED strip
		SEND_REMAPPED_PIXELS_4B(numPixels, array);
	}
}

// 3B struct input arrays
template<FAB_TDEF>
inline void
avrBitbangLedStrip<FAB_TVAR>::sendPixels(
		const uint16_t numPixels,
		const rgb * array)
{
	if (colors == RGB || colors == NONE) {
		// Input array is native format. No conversion.
		sendPixels(numPixels, (const uint8_t *) array);
	} else {
		// 4B, or 3B pixel array with different byte order, must be converted.
		SEND_REMAPPED_PIXELS_3B(numPixels, array);
	}
}

template<FAB_TDEF>
inline void
avrBitbangLedStrip<FAB_TVAR>::sendPixels(
		const uint16_t numPixels,
		const grb * array)
{
	if (colors == GRB || colors == NONE) {
		sendPixels(numPixels, (const uint8_t *) array);
	} else {
		SEND_REMAPPED_PIXELS_3B(numPixels, array);
	}
}

template<FAB_TDEF>
inline void
avrBitbangLedStrip<FAB_TVAR>::sendPixels(
		const uint16_t numPixels,
		const bgr * array)
{
	if (colors == BGR || colors == NONE) {
		sendPixels(numPixels, (const uint8_t *) array);
	} else {
		SEND_REMAPPED_PIXELS_3B(numPixels, array);
	}
}

// 4B raw input array
template<FAB_TDEF>
inline void
avrBitbangLedStrip<FAB_TVAR>::sendPixels(
		const uint16_t numPixels,
		const uint32_t * pixelArray)
{
 	DISABLE_INTERRUPTS;

	if (colors >= PIXEL_FORMAT_4B) {
		// 4 byte per pixel array, send all bytes.
		sendBytes((const uint16_t) numPixels * bytesPerPixel, (const uint8_t *) pixelArray);
	} else {
		// 3 byte per pixel array, send 3 out of 4 bytes.
		for (int i=0; i< numPixels; i++) {
			uint8_t * bytes = (uint8_t *) & pixelArray[i];
			// For LED strips using 3 bytes per color, drop a byte.
			sendBytes(bytesPerPixel, bytes);
		}
	}

	RESTORE_INTERRUPTS;
}

// Palette input arrays
template<FAB_TDEF>
template <const uint8_t bitsPerPixel, class T>
inline void
avrBitbangLedStrip<FAB_TVAR>::sendPixels (
		const uint16_t count,
		const uint8_t * pixelArray,
		const uint8_t * reds,
		const uint8_t * greens,
		const uint8_t * blues)
{
	// Debug: Support simple palettes 2, 4, 16 or 256 colors

	STATIC_ASSERT( bitsPerPixel == 1 || bitsPerPixel == 2 ||
		 bitsPerPixel == 4 || bitsPerPixel == 8,
		Unsupported_palette_size);

	// 1,2 4 and 8 bit bitmasks. Note 3,5,6 don't make sense
	// and 5bits is handled separately with uint16_t type.
	const uint8_t andMask = (bitsPerPixel ==1) ? 0x01 :
			(bitsPerPixel == 2) ? 0x03 :
			(bitsPerPixel == 4) ? 0x0F :
			(bitsPerPixel == 8) ? 0xFF :
			0x00;

 	DISABLE_INTERRUPTS;

	// Send each byte as 1 to 4 pixels
	uint16_t offset;
	offset = 0;
	uint16_t index;
	index = 0;
	while (1) {
		uint8_t elem; 
		elem = pixelArray[offset++]; 
		for (uint8_t j = 0; j < 8/bitsPerPixel; j++) {
			if (index++ >= count) {
				goto end;
			}
			const uint8_t colorIndex = elem & andMask;
			T pixel;
			pixel.r = reds[colorIndex];
			pixel.g = greens[colorIndex];
			pixel.b = blues[colorIndex];
			// @note support w in future
			sendPixels(1, &pixel);
			elem >>= bitsPerPixel;
		}
	}
end:
	RESTORE_INTERRUPTS;
}

// Palette input arrays
template<FAB_TDEF>
template <const uint8_t bitsPerPixel>
inline void
avrBitbangLedStrip<FAB_TVAR>::sendPixels (
		const uint16_t count,
		const uint8_t * pixelArray,
		const uint8_t * palette)
{
	// Debug: Support simple palettes 2, 4, 16 or 256 colors
	STATIC_ASSERT( bitsPerPixel == 1 || bitsPerPixel == 2 ||
		 bitsPerPixel == 4 || bitsPerPixel == 8,
		Unsupported_palette_size);

	// 1,2 4 and 8 bit bitmasks. Note 3,5,6 don't make sense
	// and 5bits is handled separately with uint16_t type.
	const uint8_t andMask = (bitsPerPixel ==1) ? 0x01 :
			(bitsPerPixel == 2) ? 0x03 :
			(bitsPerPixel == 4) ? 0x0F :
			(bitsPerPixel == 8) ? 0xFF :
			0x00;

 	DISABLE_INTERRUPTS;

	// Send each byte as 1 to 4 pixels
	uint16_t offset;
	offset = 0;
	uint16_t index;
	index = 0;
	while (1) {
		uint8_t elem; 
		elem = pixelArray[offset++]; 
		for (uint8_t j = 0; j < 8/bitsPerPixel; j++) {
			if (index++ >= count) {
				goto end;
			}
			const uint8_t colorIndex = elem & andMask;
			sendBytes(bytesPerPixel, &palette[bytesPerPixel*colorIndex]);
			elem >>= bitsPerPixel;
		}
	}
end:
	RESTORE_INTERRUPTS;
}

template<FAB_TDEF>
template <const uint8_t bitsPerPixel, class T>
inline void
avrBitbangLedStrip<FAB_TVAR>::sendPixels (
		const uint16_t count,
		const uint8_t * pixelArray,
		const T * palette)
{
	// Debug: Support simple palettes 2, 4, 16 or 256 colors
	STATIC_ASSERT( bitsPerPixel == 1 || bitsPerPixel == 2 ||
		bitsPerPixel == 4 || bitsPerPixel == 8,
		Unsupported_palette_size);

	// 1,2 4 and 8 bit bitmasks. Note 3,5,6 don't make sense
	// and 5bits is handled separately with uint16_t type.
	const uint8_t andMask = (bitsPerPixel ==1) ? 0x01 :
			(bitsPerPixel == 2) ? 0x03 :
			(bitsPerPixel == 4) ? 0x0F :
			(bitsPerPixel == 8) ? 0xFF :
			0x00;

 	DISABLE_INTERRUPTS;

	// Send each byte as 1 to 4 pixels
	uint16_t offset;
	offset = 0;
	uint16_t index;
	index = 0;
	while (1) {
		uint8_t elem; 
		elem = pixelArray[offset++]; 
		for (uint8_t j = 0; j < 8/bitsPerPixel; j++) {
			if (index++ >= count) {
				goto end;
			}
			const uint8_t colorIndex = elem & andMask;
			sendBytes(bytesPerPixel, &palette[bytesPerPixel*colorIndex]);
			elem >>= bitsPerPixel;
		}
	}
end:
	RESTORE_INTERRUPTS;
}

template<FAB_TDEF>
template <class pixelType> 
inline void
avrBitbangLedStrip<FAB_TVAR>::sendPixelsRemap(
		const uint16_t numPixels,
		const uint16_t * pixelMap,
		const pixelType * array)
{
	// The uint8_t raw type actually does not hold the whole pixel, it needs 3 bytes.
	const uint16_t size = (sizeof(pixelType) == 1) ? bytesPerPixel : 1;

 	DISABLE_INTERRUPTS;
	for (uint16_t i = 0; i < numPixels; i += 1) {
		const uint16_t ri = pixelMap[i];
		sendPixels((uint16_t) 1, &array[size * ri]);
	}
	RESTORE_INTERRUPTS;
}

// @note The 8bit per pixel works but 2bits per pixel does not at 16MHz.
template<FAB_TDEF>
template <const uint8_t bitsPerPixel, class pixelType>
inline void
avrBitbangLedStrip<FAB_TVAR>::sendPixelsRemap (
		const uint16_t numPixels,
		const uint16_t * pixelMap,
		const uint8_t * pixelArray,
		const pixelType * palette)
{
	// Support only palettes with 2, 4, 16 or 256 colors
	STATIC_ASSERT( bitsPerPixel == 1 || bitsPerPixel == 2 ||
		bitsPerPixel == 4 || bitsPerPixel == 8,
		Unsupported_palette_size);

	const uint16_t size = (sizeof(pixelType) == 1) ? bytesPerPixel : 1;

 	DISABLE_INTERRUPTS;
	for (uint16_t i = 0; i < numPixels; i++) {
		// Remapped i: index in array of the next pixel to push.
		const uint16_t ri = pixelMap[i];
		// Extract color index via bitmasks
		const uint8_t colorIndex = GET_PIXEL(pixelArray, ri, bitsPerPixel);
		// Get the color/pixel to send
		const pixelType * pixel =  &palette[size * colorIndex];
		sendPixels(1, pixel);
	}
	RESTORE_INTERRUPTS;
}


template<FAB_TDEF>
template <const uint8_t bitsPerPixel, class pixelType>
inline void
avrBitbangLedStrip<FAB_TVAR>::sendPixelsRemap (
		const uint8_t numPixels,
		const uint8_t * pixelMap,
		const uint8_t * pixelArray,
		const pixelType * palette)
{
	// Support only palettes with 2, 4, 16 or 256 colors
	STATIC_ASSERT( bitsPerPixel == 1 || bitsPerPixel == 2 ||
		bitsPerPixel == 4 || bitsPerPixel == 8,
		Unsupported_palette_size);

	const uint16_t size = (sizeof(pixelType) == 1) ? bytesPerPixel : 1;

 	DISABLE_INTERRUPTS;
	for (uint8_t i = 0; i < numPixels; i++) {
		// Remapped i: index in array of the next pixel to push.
		const uint8_t ri = pixelMap[i];
		// Extract color index via bitmasks
		const uint8_t colorIndex = GET_PIXEL(pixelArray, ri, bitsPerPixel);
		// Get the color/pixel to send
		const pixelType * pixel =  &palette[size * colorIndex];
		sendPixels(1, pixel);
	}
	RESTORE_INTERRUPTS;
}


/// @todo Rewrite this to support R5G6B5, R4G4B4W4 and a variable brightness.
template<FAB_TDEF>
template <uint8_t brightness>
inline void
avrBitbangLedStrip<FAB_TVAR>::sendPixels(
		int count,
		uint16_t * pixelArray)
{
	const uint8_t mask5 = ((1 << (5 - brightness))-1) || ((1 << brightness) - 1);
	const uint8_t mask10 = ((1 << (10 - brightness))-1) || ((1 << brightness) - 1);
	uint8_t bytes[4];

	// Debug: Support brightness 0..3
	STATIC_ASSERT(brightness < 3, Unsupported_brightness_level);

 	DISABLE_INTERRUPTS;

	bytes[3] = 0;
	for (int i = 0; i < count; i++) {
		const uint16_t elem = pixelArray[i]; 
		bytes[0] = (uint8_t) elem << brightness;
		bytes[1] = (elem >> (5 - brightness)) & mask5;
		bytes[2] = (elem >> (10 - brightness)) & mask10;
		sendBytes(bytesPerPixel, bytes);
	}

	RESTORE_INTERRUPTS;
}
************************************************************************************************************************************/



////////////////////////////////////////////////////////////////////////////////
// Implementation classes for LED strip
// Defines the actual LED timings
// WS2811 2811B 2812B 2812S 2801 LEDs use similar protocols and timings
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// WS2812B (default, mainstream)
////////////////////////////////////////////////////////////////////////////////
#if 0
// These are the less agressive bitbanging timings
#define WS2812B_1H_CY CYCLES(650)  // 650ns-950ns _----------__
#define WS2812B_1L_CY CYCLES(125)  // 250ns-550ns .    .    .
#define WS2812B_0H_CY CYCLES(125)  // 250ns-550ns _-----_______
#define WS2812B_0L_CY CYCLES(650)  // 650ns-950ns .    .    .
#define WS2812B_MS_REFRESH 50      // 50,000ns Minimum sleep time to reset LED strip
#define WS2812B_NS_RF 2000000      // Max refresh rate for all pixels to light up 2msec (LED PWM is 500Hz)
#else
// These are the more agressive bitbanging timings, note 0 and 1 have different durations
#define WS2812B_1H_CY CYCLES(500)  // 6  7 10 _----------__
#define WS2812B_1L_CY CYCLES(125)  // 2  2  4 .    .    .
#define WS2812B_0H_CY CYCLES(125)  // 2  2  5 _-----_______
#define WS2812B_0L_CY CYCLES(188)  // 2  2  7 .    .    .
#define WS2812B_MS_REFRESH 20      // Minimum sleep time (low) to reset LED strip
#define WS2812B_NS_RF 2000000      // Max refresh rate for all pixels to light up 2msec (LED PWM is 500Hz)
#endif

#define FAB_TVAR_WS2812B WS2812B_1H_CY, WS2812B_1L_CY, WS2812B_0H_CY, \
	WS2812B_0L_CY, WS2812B_MS_REFRESH, dataPortId, dataPortBit, A, 0, grb, ONE_PORT_BITBANG
template<avrLedStripPort dataPortId, uint8_t dataPortBit>
class ws2812b : public avrBitbangLedStrip<FAB_TVAR_WS2812B>
{
	public:
	ws2812b() : avrBitbangLedStrip<FAB_TVAR_WS2812B>() {};
	~ws2812b() {};
};
#undef FAB_TVAR_WS2812B


////////////////////////////////////////////////////////////////////////////////
// WS2812BS - Bitbang the pixels to two ports in parallel.
// The pixel array is split in two. Each port displays a half.
////////////////////////////////////////////////////////////////////////////////
#define FAB_TVAR_WS2812BS WS2812B_1H_CY, WS2812B_1L_CY, WS2812B_0H_CY, \
	WS2812B_0L_CY, WS2812B_MS_REFRESH, dataPortId1, dataPortBit1, dataPortId2, dataPortBit2, grb, TWO_PORT_SPLIT_BITBANG
template<avrLedStripPort dataPortId1, uint8_t dataPortBit1,avrLedStripPort dataPortId2, uint8_t dataPortBit2>
class ws2812bs : public avrBitbangLedStrip<FAB_TVAR_WS2812BS>
{
	public:
	ws2812bs() : avrBitbangLedStrip<FAB_TVAR_WS2812BS>() {};
	~ws2812bs() {};
};
#undef FAB_TVAR_WS2812BS

////////////////////////////////////////////////////////////////////////////////
// WS2812B8S - Bitbang the pixels to eight ports in parallel.
// The pixel array is split in 8. Each port displays a portion.
////////////////////////////////////////////////////////////////////////////////
#define FAB_TVAR_WS2812B8S WS2812B_1H_CY, WS2812B_1L_CY, WS2812B_0H_CY, \
	WS2812B_0L_CY, WS2812B_MS_REFRESH, dataPortId, dataPortBit1, dataPortId, dataPortBit2, grb, EIGHT_PORT_BITBANG
template<avrLedStripPort dataPortId, uint8_t dataPortBit1, uint8_t dataPortBit2>
class ws2812b8s : public avrBitbangLedStrip<FAB_TVAR_WS2812B8S>
{
	public:
	ws2812b8s() : avrBitbangLedStrip<FAB_TVAR_WS2812B8S>() {};
	~ws2812b8s() {};
};
#undef FAB_TVAR_WS2812B8S


////////////////////////////////////////////////////////////////////////////////
// WS2812BI - Bitbang the pixels to two ports in parallel.
// The pixels array is interleaved. One port displays the odd pixels, while the
// other portdisplays the even pixels.
////////////////////////////////////////////////////////////////////////////////
#define FAB_TVAR_WS2812BI WS2812B_1H_CY, WS2812B_1L_CY, WS2812B_0H_CY, \
	WS2812B_0L_CY, WS2812B_MS_REFRESH, dataPortId1, dataPortBit1, dataPortId2, dataPortBit2, grb, TWO_PORT_INTLV_BITBANG
template<avrLedStripPort dataPortId1, uint8_t dataPortBit1,avrLedStripPort dataPortId2, uint8_t dataPortBit2>
class ws2812bi : public avrBitbangLedStrip<FAB_TVAR_WS2812BI>
{
	public:
	ws2812bi() : avrBitbangLedStrip<FAB_TVAR_WS2812BI>() {};
	~ws2812bi() {};
};
#undef FAB_TVAR_WS2812BI


////////////////////////////////////////////////////////////////////////////////
// WS2812 (1st generation of LEDs)
////////////////////////////////////////////////////////////////////////////////
#define WS2812_1H_CY CYCLES(550)  // 500ns 550ns-850ns  _----------__
#define WS2812_1L_CY CYCLES(200)  // 125ns 200ns-500ns  .    .    .
#define WS2812_0H_CY CYCLES(200)  // 125ns 200ns-500ns  _-----_______
#define WS2812_0L_CY CYCLES(550)  // 500ns 550ns-850ns  .    .    .
#define WS2812_MS_REFRESH 50      //  50,000ns Minimum wait time to reset LED strip
#define WS2812_NS_RF 5000000      // Max refresh rate for all pixels to light up 2msec (LED PWM is 500Hz)
#define FAB_TVAR_WS2812 WS2812_1H_CY, WS2812_1L_CY, WS2812_0H_CY, \
	WS2812_0L_CY, WS2812_MS_REFRESH, dataPortId, dataPortBit, A, 0, grb, ONE_PORT_BITBANG
template<avrLedStripPort dataPortId, uint8_t dataPortBit>
class ws2812 : public avrBitbangLedStrip<FAB_TVAR_WS2812>
{
	public:
	ws2812() : avrBitbangLedStrip<FAB_TVAR_WS2812>() {};
	~ws2812() {};
};
#undef FAB_TVAR_WS2812


////////////////////////////////////////////////////////////////////////////////
// APA104 (newer better defined timings)
////////////////////////////////////////////////////////////////////////////////
#define APA104_1H_CY CYCLES(1210) // 500ns 1210ns-1510ns _----------__
#define APA104_1L_CY CYCLES(200)  // 125ns  200ns-500ns  .    .    .
#define APA104_0H_CY CYCLES(200)  // 125ns  200ns-500ns  _-----_______
#define APA104_0L_CY CYCLES(1210) // 500ns 1210ns-1510ns .    .    .
#define APA104_MS_REFRESH 50      //  50,000ns Minimum wait time to reset LED strip
#define APA104_NS_RF 5000000      // Max refresh rate for all pixels to light up 2msec (LED PWM is 500Hz)
#define FAB_TVAR_APA104 APA104_1H_CY, APA104_1L_CY, APA104_0H_CY, \
	APA104_0L_CY, APA104_MS_REFRESH, dataPortId, dataPortBit, A, 0, grb, ONE_PORT_BITBANG
template<avrLedStripPort dataPortId, uint8_t dataPortBit>
class apa104 : public avrBitbangLedStrip<FAB_TVAR_APA104>
{
	public:
	apa104() : avrBitbangLedStrip<FAB_TVAR_APA104>() {};
	~apa104() {};
};
#undef FAB_TVAR_APA104
#define pl9823 apa104; 


////////////////////////////////////////////////////////////////////////////////
// APA106 (like APA104, but with LEDs ordered as RGB instead of GRB)
////////////////////////////////////////////////////////////////////////////////
#define APA106_1H_CY CYCLES(1210) // 500ns 1210ns-1510ns _----------__
#define APA106_1L_CY CYCLES(200)  // 125ns  200ns-500ns  .    .    .
#define APA106_0H_CY CYCLES(200)  // 125ns  200ns-500ns  _-----_______
#define APA106_0L_CY CYCLES(1210) // 500ns 1210ns-1510ns .    .    .
#define APA106_MS_REFRESH 50      //  50,000ns Minimum wait time to reset LED strip
#define APA106_NS_RF 5000000      // Max refresh rate for all pixels to light up 2msec (LED PWM is 500Hz)
#define FAB_TVAR_APA106 APA106_1H_CY, APA106_1L_CY, APA106_0H_CY, \
	APA106_0L_CY, APA106_MS_REFRESH, dataPortId, dataPortBit, A, 0, rgb, ONE_PORT_BITBANG
template<avrLedStripPort dataPortId, uint8_t dataPortBit>
class apa106 : public avrBitbangLedStrip<FAB_TVAR_APA106>
{
	public:
	apa106() : avrBitbangLedStrip<FAB_TVAR_APA106>() {};
	~apa106() {};
};
#undef FAB_TVAR_APA106



////////////////////////////////////////////////////////////////////////////////
// SK6812 (4 color RGBW LEDs, faster PWM frequency, updated timings)
/// @note I need to use microsleep to not round up sleep to 1msec.
////////////////////////////////////////////////////////////////////////////////
#define SK6812_1H_CY CYCLES(1210) // 500ns 1210ns-1510ns _----------__
#define SK6812_1L_CY CYCLES(200)  // 125ns  200ns-500ns  .    .    .
#define SK6812_0H_CY CYCLES(200)  // 125ns  200ns-500ns  _-----_______
#define SK6812_0L_CY CYCLES(1210) // 500ns 1210ns-1510ns .    .    .
#define SK6812_MS_REFRESH 84      //  84,000ns Minimum wait time to reset LED strip
#define SK6812_NS_RF  833333      // Max refresh rate for all pixels to light up 2msec (LED PWM is 500Hz)
#define FAB_TVAR_SK6812 SK6812_1H_CY, SK6812_1L_CY, SK6812_0H_CY, \
	SK6812_0L_CY, SK6812_MS_REFRESH, dataPortId, dataPortBit, A, 0, rgbw, ONE_PORT_BITBANG
template<avrLedStripPort dataPortId, uint8_t dataPortBit>
class sk6812 : public avrBitbangLedStrip<FAB_TVAR_SK6812>
{
	public:
	sk6812() : avrBitbangLedStrip<FAB_TVAR_SK6812>() {};
	~sk6812() {};
};
#undef FAB_TVAR_SK6812




////////////////////////////////////////////////////////////////////////////////
// SK6812B (4 color GRBW LEDs, faster PWM frequency, updated timings)
/// @note I need to use microsleep to not round up sleep to 1msec.
////////////////////////////////////////////////////////////////////////////////
#define SK6812B_1H_CY CYCLES(1210) // 500ns 1210ns-1510ns _----------__
#define SK6812B_1L_CY CYCLES(200)  // 125ns  200ns-500ns  .    .    .
#define SK6812B_0H_CY CYCLES(200)  // 125ns  200ns-500ns  _-----_______
#define SK6812B_0L_CY CYCLES(1210) // 500ns 1210ns-1510ns .    .    .
#define SK6812B_MS_REFRESH 84      //  84,000ns Minimum wait time to reset LED strip
#define SK6812B_NS_RF  833333      // Max refresh rate for all pixels to light up 2msec (LED PWM is 500Hz)
#define FAB_TVAR_SK6812B SK6812B_1H_CY, SK6812B_1L_CY, SK6812B_0H_CY, \
	SK6812B_0L_CY, SK6812B_MS_REFRESH, dataPortId, dataPortBit, A, 0, grbw, ONE_PORT_BITBANG
template<avrLedStripPort dataPortId, uint8_t dataPortBit>
class sk6812b : public avrBitbangLedStrip<FAB_TVAR_SK6812B>
{
	public:
	sk6812b() : avrBitbangLedStrip<FAB_TVAR_SK6812B>() {};
	~sk6812b() {};
};
#undef FAB_TVAR_SK6812B


////////////////////////////////////////////////////////////////////////////////
// APA-102 (3 color RGB LEDs, SPI protocol)
////////////////////////////////////////////////////////////////////////////////
#define APA102_1H_CY CYCLES(0) // Unused
#define APA102_1L_CY CYCLES(0)  // Unused
#define APA102_0H_CY CYCLES(0)  // Unused
#define APA102_0L_CY CYCLES(0) // Unused
#define APA102_MS_REFRESH 84      //  84,000ns Minimum wait time to reset LED strip
#define APA102_NS_RF  833333      // Max refresh rate for all pixels to light up 2msec (LED PWM is 500Hz)
#define FAB_TVAR_APA102 APA102_1H_CY, APA102_1L_CY, APA102_0H_CY, \
	APA102_0L_CY, APA102_MS_REFRESH, dataPortId, dataPortBit, clockPortId, clockPortBit, hbgr, SPI_BITBANG
template<avrLedStripPort dataPortId, uint8_t dataPortBit, avrLedStripPort clockPortId, uint8_t clockPortBit>
class apa102 : public avrBitbangLedStrip<FAB_TVAR_APA102>
{
	public:
	apa102() : avrBitbangLedStrip<FAB_TVAR_APA102>() {};
	~apa102() {};
};
#undef FAB_TVAR_APA102

#endif // FAB_LED_H
