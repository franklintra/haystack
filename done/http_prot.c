/**
 * @file http_prot.c
 * @brief Implementation of HTTP protocol related functions for the CS-202 project
 *
 * Contains some functions for parsing and handling the HTTP messages, including matching URIs, verbs,
 * extracting variables from URLs, and also parsing HTTP messages and headers.
 */

#include <string.h>
#include "http_prot.h"
#include "error.h"
#include <stdlib.h>

/**
 * @brief Matches the URI of an HTTP message with a target URI.
 *
 * Compares the URI of the given HTTP message with the target URI.
 * It checks if the prefix of the message URI matches the target URI.
 *
 * @param message Pointer to the HTTP message structure.
 * @param target_uri Target URI to match.
 * @return 1 if the target URI is matching the message URI prefix, 0 otherwise.
 */
int http_match_uri(const struct http_message *message, const char *target_uri)
{
    M_REQUIRE_NON_NULL(message);
    M_REQUIRE_NON_NULL(target_uri);
    size_t target_len = strlen(target_uri);
    return strncmp(message->uri.val, target_uri, target_len) == 0;
}

/**
 * @brief Matches the HTTP method with a given verb.
 *
 * Compares the HTTP method with the given verb.
 * It checks if the entire method string matches the verb.
 *
 * @param method Points the HTTP method string structure.
 * @param verb The verb to match.
 * @return 1 if the method matches the verb, 0 otherwise.
 */
int http_match_verb(const struct http_string* method, const char* verb)
{
    M_REQUIRE_NON_NULL(method);
    M_REQUIRE_NON_NULL(verb);
    return method->len == strlen(verb) && strncmp(method->val, verb, method->len) == 0;
}

/**
 * @brief Extracts the value of a parameter from a URL.
 *
 * This function extracts the value of a given parameter from the URL.
 * It looks for the parameter name and extracts its value if found.
 *
 * @param url Pointer to the URL string structure.
 * @param name The parameter name to extract.
 * @param out Buffer to store the extracted value.
 * @param out_len Length of the output buffer.
 * @return Length of the extracted value if found, ERR_RUNTIME if the buffer is too small, 0 if not found.
 */
int http_get_var(const struct http_string* url, const char* name, char* out, size_t out_len)
{
    M_REQUIRE_NON_NULL(url);
    M_REQUIRE_NON_NULL(name);
    M_REQUIRE_NON_NULL(out);

    const char *query_start = strchr(url->val, '?');
    if (!query_start) return 0; // it means that there is no query in url

    char param[strlen(name) + 2];
    snprintf(param, sizeof(param), "%s=", name);

    const char *start = strstr(query_start, param);
    while (start) {
        // check if the parameter is right after '?' or '&'
        if (start == query_start + 1 || *(start - 1) == '&') {
            start += strlen(param);
            const char *end = strchr(start, '&');
            size_t len = end ? (size_t)(end - start) : (url->len - (size_t)(start - url->val));
            if (len >= out_len) return ERR_RUNTIME;
            strncpy(out, start, len);
            out[len] = '\0';
            return (int)len;
        }
        start = strstr(start + 1, param);
    }
    return 0; // Param not found
}

/**
 * @brief Extracts the next token from a message string.
 *
 * Extracts the first substring (token) of the message before a specified delimiter.
 * It also updates the output HTTP string structure with the token value and length.
 *
 * @param message The input message string.
 * @param delimiter What delimits the string to split the message.
 * @param output Pointer to the HTTP string structure to store the token.
 * @return Pointer to the remaining part of the message after the delimiter.
 */
static const char* get_next_token(const char* message, const char* delimiter, struct http_string* output)
{
    const char *end = strstr(message, delimiter);
    if (!end) {
        debug_printf("Delimiter '%s' not found\n", delimiter);
        return NULL;
    }
    if (output) {
        output->val = message;
        output->len = (size_t)(end - message);
    }
    return end + strlen(delimiter);
}

/**
 * @brief Parses HTTP headers from the header section of an HTTP message.
 *
 * It parses the headers (from the header section) of an HTTP message.
 * And it also extracts key-value pairs of headers and stores them in the HTTP message structure.
 *
 * @param header_start Pointer to the start of the header section.
 * @param output Pointer to the HTTP message structure.
 * @return Pointer to the start of the body section.
 */
static const char* http_parse_headers(const char* header_start, struct http_message* output)
{

    debug_printf("Parsing headers\n", NULL);
    const char *current = header_start;

    while (current && *current != '\0' && strncmp(current, HTTP_HDR_END_DELIM, strlen(HTTP_HDR_END_DELIM)) != 0) {
        if (output->num_headers >= MAX_HEADERS) {
            perror("Too many headers\n");
            return NULL;
        }

        // Find the end of the current header line
        const char *line_end = strstr(current, HTTP_LINE_DELIM);
        if (!line_end) {
            perror("Line delimiter not found\n");
            return NULL;
        }

        // If the line is empty, we have to skip it
        if (line_end == current) {
            return current + strlen(HTTP_LINE_DELIM);
        }
        debug_printf("Current header start: %.*s\n", (int)(line_end - current), current);

        // Extract the header key
        current = get_next_token(current, HTTP_HDR_KV_DELIM, &output->headers[output->num_headers].key);
        if (!current || current >= line_end) {
            perror("Failed to get header key\n");
            return NULL;
        }
        debug_printf("Header key: %.*s\n", (int)output->headers[output->num_headers].key.len,
                     output->headers[output->num_headers].key.val);

        // Extract the header value
        current = get_next_token(current, HTTP_LINE_DELIM, &output->headers[output->num_headers].value);
        if (!current || current > line_end + strlen(HTTP_LINE_DELIM)) {
            perror("Failed to get header value\n");
            return NULL;
        }


        debug_printf("Header value: %.*s\n", (int)output->headers[output->num_headers].value.len,
                     output->headers[output->num_headers].value.val);

        output->num_headers++;
        current = line_end + strlen(HTTP_LINE_DELIM);
    }

    // Return the pointer to the start of the body section after the headers
    return current ? current + strlen(HTTP_HDR_END_DELIM) : NULL;
}

/**
 * @brief Parses an HTTP message.
 *
 * This function parses a complete HTTP message, extracting the method, URI, headers, and body.
 *
 * @param stream The input stream.
 * @param bytes_received Number of bytes received in the message.
 * @param out Pointer to the HTTP message structure to store parsed data.
 * @param content_len Pointer to store the length of the message body.
 * @return 1 if the message is fully parsed, 0 if the message is incomplete, error code otherwise.
 */
int http_parse_message(const char *stream, size_t bytes_received, struct http_message *out, int *content_len)
{
    if (!stream || !out || !content_len) {
        perror("Invalid argument: NULL pointer passed to http_parse_message\n");
        return ERR_INVALID_ARGUMENT;
    }

    const char *current = stream;

    // Check if headers are completely received
    const char *headers_end = strstr(stream, HTTP_HDR_END_DELIM);
    if (!headers_end) {
        perror("Headers are not fully received\n");
        return 0;
    }

    // Parse the start line
    current = get_next_token(current, " ", &out->method);
    if (!current || out->method.len == 0) {
        perror("Failed to parse HTTP method\n");
        return 0;
    }

    current = get_next_token(current, " ", &out->uri);
    if (!current || out->uri.len == 0) {
        perror("Failed to parse HTTP URI\n");
        return 0;
    }

    // Skip the HTTP version
    current = get_next_token(current, HTTP_LINE_DELIM, NULL);
    if (!current) {
        perror("Failed to parse HTTP version\n");
        return 0;
    }

    // reset header numbers
    out->num_headers = 0;

    // parse headers
    current = http_parse_headers(current, out);
    if (!current) {
        perror("Failed to parse HTTP headers\n");
        return 0;
    }

    // print headers
    for (size_t i = 0; i < out->num_headers; i++) {
        debug_printf("Header %zu: %.*s: %.*s\n", i, (int)out->headers[i].key.len, out->headers[i].key.val,
                     (int)out->headers[i].value.len, out->headers[i].value.val);
    }

    // find the content length
    *content_len = 0;
    for (size_t i = 0; i < out->num_headers; i++) {
        if (strncmp(out->headers[i].key.val, "Content-Length", out->headers[i].key.len) == 0) {
            *content_len = atoi(out->headers[i].value.val);
            break;
        }
    }

    // calculate the length of the headers
    size_t headers_length = (size_t) (current - stream);

    // check if body has been fully received
    debug_printf("Bytes received: %zu, Headers length: %zu, Content length: %d\n", bytes_received, headers_length, *content_len);
    if (bytes_received < (headers_length + (size_t)(*content_len))) {
        perror("The body is not fully received\n");
        return 0;
    }

    // set the body to the appropriate field
    if (*content_len > 0) {
        out->body.val = current;
        out->body.len = (size_t)*content_len;
    }

    return 1;
}
