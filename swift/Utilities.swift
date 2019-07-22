//
// Copyright (c) 2019 William Kent. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
// 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
//     its contributors may be used to endorse or promote products derived
//     from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY ITS CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

import Foundation

internal struct StandardErrorWriter: TextOutputStream {
	mutating func write(_ string: String) {
		fputs(string, stderr)
	}
}
internal var standardError = StandardErrorWriter()

internal extension CommandLine {
	static var workingDirectory: String {
		get {
			let buffer = UnsafeMutablePointer<Int8>.allocate(capacity: Int(MAXPATHLEN) * MemoryLayout<Int8>.stride)
			defer {
				buffer.deallocate()
			}

			getcwd(buffer, Int(MAXPATHLEN))
			return String(cString: UnsafePointer<UInt8>(OpaquePointer(buffer)))
		}

		set {
			_ = newValue.withCString {
				buffer in
				chdir(buffer)
			}
		}
	}

	enum Environment {
		internal static subscript(name: String) -> String? {
			get {
				let buffer = name.withCString {
					nameBuffer in
					getenv(nameBuffer)
				}

				if let buffer = buffer {
					return String(cString: buffer)
				} else {
					return nil
				}
			}

			set {
				if let newValue = newValue {
					_ = name.withCString {
						nameBuffer in
						newValue.withCString {
							valueBuffer in
							setenv(nameBuffer, valueBuffer, 1)
						}
					}
				} else {
					_ = name.withCString {
						nameBuffer in
						unsetenv(nameBuffer)
					}
				}
			}
		}
	}
}

internal extension FileManager {
	func directoryExists(atPath path: String) -> Bool {
		var isDir = ObjCBool(false)
		let exists = self.fileExists(atPath: path, isDirectory: &isDir)
		return exists && isDir.boolValue
	}

	func fileExists(atPath path: String) -> Bool {
		var isDir = ObjCBool(false)
		let exists = self.fileExists(atPath: path, isDirectory: &isDir)
		return exists && !isDir.boolValue
	}
}

internal func joinPath(_ paths: String...) -> String {
	var accumulator = URL(fileURLWithPath: paths[0])
	for element in paths[1...] {
		accumulator = accumulator.appendingPathComponent(element)
	}
	return accumulator.path
}
