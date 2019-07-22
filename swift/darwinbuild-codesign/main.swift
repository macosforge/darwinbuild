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
import SwiftCLI

class CodesignCommand: Command {
	let name = "darwinbuild-codesign"

	private let dstroot = Key<String>("--dstroot", "-d", description: "Code-sign the files in this directory")
	private let srcroot = Key<String>("--srcroot", "-s", description: "Path to the sources corresponding to the dstroot")
	private let projectName = Key<String>("--project", "-p", description: "Code-sign this darwinbuild output root")

	public func execute() throws {
		var dstroot = self.dstroot.value
		var srcroot = self.srcroot.value

		if let projectName = projectName.value {
			let buildroot = CommandLine.Environment["DARWIN_BUILDROOT"] ?? CommandLine.workingDirectory

			let fm = FileManager()
			if !(fm.directoryExists(atPath: joinPath(buildroot, "Roots")) &&
				fm.directoryExists(atPath: joinPath(buildroot, "Sources")) &&
				fm.directoryExists(atPath: joinPath(buildroot, "Symbols")) &&
				fm.directoryExists(atPath: joinPath(buildroot, "Headers")) &&
				fm.directoryExists(atPath: joinPath(buildroot, "Logs"))) {
				print("ERROR: Could not find darwinbuild root, this is required when using --project", to: &standardError)
				print("Please change your working directory to one initialized by:", to: &standardError)
				print("darwinbuild -init <plist>", to: &standardError)
				print("Alternatively, you may set the DARWIN_BUILDROOT environment variable to the", to: &standardError)
				print("absolute path of that directory.", to: &standardError)

				exit(1)
			}

			print("Not implemented...", to: &standardError)
			exit(-1)
		}
	}
}

func main() {
	let parser = CLI(singleCommand: CodesignCommand())
	parser.goAndExit()
}

main()
