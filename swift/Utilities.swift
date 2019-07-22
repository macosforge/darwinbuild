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
