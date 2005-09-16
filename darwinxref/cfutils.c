/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_BSD_LICENSE_HEADER_START@
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * @APPLE_BSD_LICENSE_HEADER_END@
 */

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <mach-o/dyld.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cfutils.h"

char* strdup_cfstr(CFStringRef str) {
        char* result = NULL;
        if (str != NULL) {
                CFIndex length = CFStringGetLength(str);
                CFIndex size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
                result = malloc(size+1);
                if (result != NULL) {
                        length = CFStringGetBytes(str, CFRangeMake(0, length), kCFStringEncodingUTF8, '?', 0, (UInt8*)result, size, NULL);
                        result[length] = 0;
                }
        }
        return result;
}

CFStringRef cfstr(const char* str) {
        CFStringRef result = NULL;
        if (str) result = CFStringCreateWithCString(NULL, str, kCFStringEncodingUTF8);
        return result;
}

void perror_cfstr(CFStringRef str) {
        char* cstr = strdup_cfstr(str);
        if (cstr) {
                fprintf(stderr, "%s", cstr);
                free(cstr);
        }
}

CFPropertyListRef read_plist(char* path) {
        CFPropertyListRef result = NULL;
        int fd = open(path, O_RDONLY, (mode_t)0);
        if (fd != -1) {
                struct stat sb;
                if (stat(path, &sb) != -1) {
                        off_t size = sb.st_size;
                        void* buffer = mmap(NULL, size, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, (off_t)0);
                        if (buffer != (void*)-1) {
                                CFDataRef data = CFDataCreateWithBytesNoCopy(NULL, buffer, size, kCFAllocatorNull);
                                if (data) {
                                        CFStringRef str = NULL;
                                        result = CFPropertyListCreateFromXMLData(NULL, data, kCFPropertyListMutableContainers, &str);
                                        CFRelease(data);
                                        if (result == NULL) {
                                                perror_cfstr(str);
                                        }
                                }
                                munmap(buffer, size);
                        } else {
                                perror(path);
                        }
                }
                close(fd);
        } else {
                perror(path);
        }
        return result;
}


int cfprintf(FILE* file, const char* format, ...) {
		char* cstr;
		int result;
        va_list args;
        va_start(args, format);
        CFStringRef formatStr = CFStringCreateWithCStringNoCopy(NULL, format, kCFStringEncodingUTF8, kCFAllocatorNull);
        CFStringRef str = CFStringCreateWithFormatAndArguments(NULL, NULL, formatStr, args);
        va_end(args);
        cstr = strdup_cfstr(str);
        result = fprintf(file, "%s", cstr);
        free(cstr);
        CFRelease(str);
        CFRelease(formatStr);
		return result;
}

CFArrayRef dictionaryGetSortedKeys(CFDictionaryRef dictionary) {
        CFIndex count = CFDictionaryGetCount(dictionary);

        const void** keys = malloc(sizeof(CFStringRef) * count);
        CFDictionaryGetKeysAndValues(dictionary, keys, NULL);
        CFArrayRef keysArray = CFArrayCreate(NULL, keys, count, &kCFTypeArrayCallBacks);
        CFMutableArrayRef sortedKeys = CFArrayCreateMutableCopy(NULL, count, keysArray);
        CFRelease(keysArray);
        free(keys);

        CFArraySortValues(sortedKeys, CFRangeMake(0, count), (CFComparatorFunction)CFStringCompare, 0);
        return sortedKeys;
}

int writePlist(FILE* f, CFPropertyListRef p, int tabs) {
		int result = 0;
        CFTypeID type = CFGetTypeID(p);

        if (tabs == 0) {
                result += fprintf(f, "// !$*UTF8*$!\n");
        }

        char* t = malloc(tabs+1);
        int i;
        for (i = 0; i < tabs; ++i) {
                t[i] = '\t';
        }
        t[tabs] = 0;

        if (type == CFStringGetTypeID()) {
                char* utf8 = strdup_cfstr(p);
                // XXX: needs work
                int quote = 0;
                for (i = 0 ;; ++i) {
                        int c = utf8[i];
                        if (c == 0) break;
                        if (!((c >= 'A' && c <= 'Z') ||
                                  (c >= 'a' && c <= 'z') ||
                                  (c >= '0' && c <= '9') ||
                                  c == '/' ||
                                  c == '.' ||
                                  c == '_' )) {
                                quote = 1;
                                break;
                        }
                }
                if (utf8[0] == 0) quote = 1;

                if (quote) result += fprintf(f, "\"");
                for (i = 0 ;; ++i) {
                        int c = utf8[i];
                        if (c == 0) break;
                        if (c == '\"' || c == '\\') fprintf(f, "\\");
                        //if (c == '\n') c = 'n';
                        result += fprintf(f, "%c", c);
                }
                if (quote) result += fprintf(f, "\"");
                free(utf8);
        } else if (type == CFArrayGetTypeID()) {
                result += fprintf(f, "(\n");
                int count = CFArrayGetCount(p);
                for (i = 0; i < count; ++i) {
                        CFTypeRef pp = CFArrayGetValueAtIndex(p, i);
                        result += fprintf(f, "%s\t", t);
                        result += writePlist(f, pp, tabs+1);
                        result += fprintf(f, ",\n");
                }
                result += fprintf(f, "%s)", t);
        } else if (type == CFDictionaryGetTypeID()) {
                result += fprintf(f, "{\n");
                CFArrayRef keys = dictionaryGetSortedKeys(p);
                int count = CFArrayGetCount(keys);
                for (i = 0; i < count; ++i) {
                        CFStringRef key = CFArrayGetValueAtIndex(keys, i);
						result += fprintf(f, "\t%s", t);
						result += writePlist(f, key, tabs+1);
                        result += fprintf(f, " = ");
                        result += writePlist(f, CFDictionaryGetValue(p,key), tabs+1);
                        result += fprintf(f, ";\n");
                }
                CFRelease(keys);
                result += fprintf(f, "%s}", t);
        }
        if (tabs == 0) result += fprintf(f, "\n");
        free(t);
		return result;
}


CFArrayRef tokenizeString(CFStringRef str) {
	CFMutableArrayRef result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	
	CFCharacterSetRef whitespace = CFCharacterSetCreateWithCharactersInString(NULL, CFSTR(" \t\n\r"));

	CFStringInlineBuffer buf;
	CFIndex i, length = CFStringGetLength(str);
	CFIndex start = 0;
	CFStringInitInlineBuffer(str, &buf, CFRangeMake(0, length));
	for (i = 0; i < length; ++i) {
		UniChar c = CFStringGetCharacterFromInlineBuffer(&buf, i);
		if (CFCharacterSetIsCharacterMember(whitespace, c)) {
			if (start != kCFNotFound) {
				CFStringRef sub = CFStringCreateWithSubstring(NULL, str, CFRangeMake(start, i - start));
				start = kCFNotFound;
				CFArrayAppendValue(result, sub);
				CFRelease(sub);
			}
		} else if (start == kCFNotFound) {
			start = i;
		}
	}
	if (start != kCFNotFound) {
		CFStringRef sub = CFStringCreateWithSubstring(NULL, str, CFRangeMake(start, i - start));
		CFArrayAppendValue(result, sub);
		CFRelease(sub);
	}

	CFRelease(whitespace);

	return result;
}

void _mergeDictionaries(const void* key, const void* value, void* dst) {
	CFDictionaryAddValue(dst, key, value);
}

CFDictionaryRef mergeDictionaries(CFDictionaryRef dst, CFDictionaryRef src) {
	CFMutableDictionaryRef res = CFDictionaryCreateMutableCopy(NULL, 0, dst);
	CFDictionaryApplyFunction(src, _mergeDictionaries, res);
	return res;
}

// appends each element that does not already exist in the array
void arrayAppendArrayDistinct(CFMutableArrayRef array, CFArrayRef other) {
	CFIndex i, count = CFArrayGetCount(other);
	CFRange range = CFRangeMake(0, CFArrayGetCount(array));
	for (i = 0; i < count; ++i) {
		CFTypeRef o = CFArrayGetValueAtIndex(other, i);
		if (!CFArrayContainsValue(array, range, o)) {
			CFArrayAppendValue(array, o);
		}
	}
}

//
// Call backs suitable for adding C strings to a CFArray
//
const void* retainCStr(CFAllocatorRef allocator, const void *value) {
	return strdup(value);
}
void releaseCStr(CFAllocatorRef allocator, const void *value) {
	free((void*)value);
}
CFStringRef	copyDescriptionCStr(const void *value) {
	return cfstr(value);
}
Boolean equalCStr(const void *value1, const void *value2) {
	return (strcmp(value1, value2) == 0);
}
CFArrayCallBacks cfArrayCStringCallBacks = {
	0, retainCStr, releaseCStr, copyDescriptionCStr, equalCStr
};

/* djb2
 * This algorithm was first reported by Dan Bernstein
 * many years ago in comp.lang.c
 */
CFHashCode hashCStr(const void *value) {
	const char* str = (const char*)value;
	unsigned long hash = 5381;
	int c; 
	while (c = *str++) hash = ((hash << 5) + hash) + c; // hash*33 + c
	return hash;
}
CFDictionaryKeyCallBacks cfDictionaryCStringKeyCallBacks = {
	0, retainCStr, releaseCStr, copyDescriptionCStr, equalCStr, hashCStr
};
CFDictionaryValueCallBacks cfDictionaryCStringValueCallBacks = {
	0, retainCStr, releaseCStr, copyDescriptionCStr, equalCStr
};
