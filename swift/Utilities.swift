//
//  Utilities.swift
//  darwinbuild-codesign
//
//  Created by William Kent on 7/21/19.
//  Copyright © 2019 The DarwinBuild Project. All rights reserved.
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
