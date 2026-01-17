#include "announce_connector.h"

#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <uriparser/Uri.h>

char* get_peers_list(const UdpAnnounceRequest* announce) {
    UriUriA announce_uri;
    const char *errorPos;
    if (uriParseSingleUriA(&announce_uri, announce->announce_address, &errorPos) != URI_SUCCESS) {
        fprintf(stderr, "Invalid URI at: %s\n", errorPos);
        return NULL;
    }

    struct addrinfo hints = {0}, *server_info;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = AI_PASSIVE;

    memset(&hints, 0, sizeof hints);

    char tracker_host[256], tracker_port[10];

    const int host_len = announce_uri.hostText.afterLast - announce_uri.hostText.first;
    const int port_len = announce_uri.portText.afterLast - announce_uri.portText.first;

    if (host_len > 0 && host_len < sizeof(tracker_host)) {
        strncpy(tracker_host, announce_uri.hostText.first, host_len);
        tracker_host[host_len] = '\0';
    }

    if (port_len > 0 && port_len < sizeof(tracker_port)) {
        strncpy(tracker_port, announce_uri.portText.first, port_len);
        tracker_port[port_len] = '\0';
    }

    const int status = getaddrinfo(tracker_host, tracker_port, &hints, &server_info);
    if (status != 0) {
        fprintf(stderr, "DNS Lookup failed: %s\n", gai_strerror(status));
        return NULL;
    }


    return NULL;
}
