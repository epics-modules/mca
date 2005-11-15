typedef int libnet_t;

#define ETH_ALEN  6               /* Octets in one ethernet addr   */

#define LIBNET_LINK     0x00            /* link-layer interface */

/* This is a name for the 48 bit ethernet address available on many
   systems.  */
struct ether_addr
{
  u_int8_t ether_addr_octet[ETH_ALEN];
} __attribute__ ((__packed__));


/* 10Mb/s ethernet header */
struct ether_header
{
  u_int8_t  ether_dhost[ETH_ALEN];      /* destination eth addr */
  u_int8_t  ether_shost[ETH_ALEN];      /* source ether addr    */
  u_int16_t ether_type;                 /* packet type ID field */
} __attribute__ ((__packed__));


/**
 * The libnet error buffer is 256 bytes long.
 */
#define LIBNET_ERRBUF_SIZE      0x100

/*
 *  Libnet ptags are how we identify specific protocol blocks inside the
 *  list.
 */
typedef int32_t libnet_ptag_t;

/**
 * Creates the libnet environment. It initializes the library and returns a
 * libnet context. If the injection_type is LIBNET_LINK or LIBNET_LINK_ADV, the
 * function initializes the injection primitives for the link-layer interface
 * enabling the application programmer to build packets starting at the
 * data-link layer (which also provides more granular control over the IP
 * layer). If libnet uses the link-layer and the device argument is non-NULL,
 * the function attempts to use the specified network device for packet
 * injection. This is either a canonical string that references the device
 * (such as "eth0" for a 100MB Ethernet card on Linux or "fxp0" for a 100MB
 * Ethernet card on OpenBSD) or the dots and decimals representation of the
 * device's IP address (192.168.0.1). If device is NULL, libnet attempts to
 * find a suitable device to use. If the injection_type is LIBNET_RAW4 or
 * LIBNET_RAW4_ADV, the function initializes the injection primitives for the
 * IPv4 raw socket interface. The final argument, err_buf, should be a buffer
 * of size LIBNET_ERRBUF_SIZE and holds an error message if the function fails.
 * This function requires root privileges to execute successfully. Upon
 * success, the function returns a valid libnet context for use in later
 * function calls; upon failure, the function returns NULL.
 * @param injection_type packet injection type (LIBNET_LINK, LIBNET_LINK_ADV, LIBNET_RAW4, LIBNET_RAW4_ADV, LIBNET_RAW6, LIBNET_RAW6_ADV)
 * @param device the interface to use (NULL and libnet will choose one)
 * @param err_buf will contain an error message on failure
 * @return libnet context ready for use or NULL on error.
 */
libnet_t *
libnet_init(int injection_type, char *device, char *err_buf);

/**
 * Returns the MAC address for the device libnet was initialized with. If
 * libnet was initialized without a device the function will attempt to find
 * one. If the function fails and returns NULL a call to libnet_geterror() will
 * tell you why.
 * @param l pointer to a libnet context
 * @return a pointer to the MAC address or NULL
 */
struct libnet_ether_addr *
libnet_get_hwaddr(libnet_t *l);

/**
 * Shuts down the libnet session referenced by l. It closes the network
 * interface and frees all internal memory structures associated with l.
 * @param l pointer to a libnet context
 */
void
libnet_destroy(libnet_t *l);

/**
 * Clears the current packet referenced and frees all pblocks. Should be
 * called when the programmer want to send a completely new packet of
 * a different type using the same context.
 * @param l pointer to a libnet context
 */
void
libnet_clear_packet(libnet_t *l);

/**
 * Builds an Ethernet header. The RFC 894 Ethernet II header is almost
 * identical to the IEEE 802.3 header, with the exception that the field
 * immediately following the source address holds the layer 3 protocol (as
 * opposed to frame's length). You should only use this function when
 * libnet is initialized with the LIBNET_LINK interface.
 * @param dst destination ethernet address
 * @param src source ethernet address
 * @param type upper layer protocol type
 * @param payload optional payload or NULL
 * @param payload_s payload length or 0
 * @param l pointer to a libnet context
 * @param ptag protocol tag to modify an existing header, 0 to build a new one
 * @return protocol tag value on success, -1 on error
 */
libnet_ptag_t
libnet_build_ethernet(u_int8_t *dst, u_int8_t *src, u_int16_t type,
u_int8_t *payload, u_int32_t payload_s, libnet_t *l, libnet_ptag_t ptag);


/**
 * Returns the last error set inside of the referenced libnet context. This
 * function should be called anytime a function fails or an error condition
 * is detected inside of libnet.
 * @param l pointer to a libnet context
 * @return an error string or NULL if no error has occured
 */
char *
libnet_geterror(libnet_t *l);

/**
 * Writes a prebuilt packet to the network. The function assumes that l was
 * previously initialized (via a call to libnet_init()) and that a
 * previously constructed packet has been built inside this context (via one or
 * more calls to the libnet_build* family of functions) and is ready to go.
 * Depending on how libnet was initialized, the function will write the packet
 * to the wire either via the raw or link layer interface. The function will
 * also bump up the internal libnet stat counters which are retrievable via
 * libnet_stats().
 * @param l pointer to a libnet context
 * @return the number of bytes written, -1 on error
 */
int
libnet_write(libnet_t *l);

