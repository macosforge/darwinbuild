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
}
