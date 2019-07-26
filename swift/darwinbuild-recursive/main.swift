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

fileprivate func readProcessOutput(commandName: String, arguments: [String]) -> String {
	let output = CaptureStream()
	let task = Task(executable: commandName, arguments: arguments, stdout: output, stderr: WriteStream.stderr)

	let exitCode = task.runSync()
	if exitCode != 0 {
		print("\(commandName) \(arguments) failed with code \(exitCode)", to: &standardError)
		exit(1)
	}

	if let text = String(data: output.readAllData(), encoding: .utf8) {
		return text
	} else {
		fatalError("could not decode tool output into UTF-8 string")
	}
}

fileprivate func splitLines(_ input: String) -> [String] {
	var retval: [String] = []
	input.enumerateLines {
		(line, _) in
		retval.append(line)
	}
	return retval
}

fileprivate struct RecursiveBuildContext {
	fileprivate var completedDependencies = Set<String>()
	fileprivate var completedHeaderDependencies = Set<String>()

	fileprivate mutating func buildProject(projectName: String, isHeaderDependency: Bool = false) {
		if isHeaderDependency {
			if completedHeaderDependencies.contains(projectName) {
				return
			} else {
				completedHeaderDependencies.insert(projectName)
			}
		} else {
			if completedDependencies.contains(projectName) {
				return
			} else {
				completedDependencies.insert(projectName)
			}
		}

		let buildDependencies = splitLines(readProcessOutput(commandName: "/usr/local/bin/darwinxref", arguments: ["dependencies", "-build", projectName]))
		let headerDependencies = splitLines(readProcessOutput(commandName: "/usr/local/bin/darwinxref", arguments: ["dependencies", "-header", projectName]))

		for dep in headerDependencies {
			buildProject(projectName: dep, isHeaderDependency: true)
		}
		for dep in buildDependencies {
			buildProject(projectName: dep, isHeaderDependency: false)
		}

		let task = Task(executable: "/usr/local/bin/darwinbuild", arguments: isHeaderDependency ? ["-headers", projectName] : [projectName], stdout: WriteStream.stdout, stderr: WriteStream.stderr)
		let exitCode = task.runSync()
		if exitCode != 0 {
			print("darwinbuild \(isHeaderDependency ? "-headers " : "")\(projectName) failed with code \(exitCode)", to: &standardError)
			exit(1)
		}
	}
}

func main() {
	if CommandLine.arguments.count < 3 {
		print("Internal tool used by darwinbuild, please do not invoke directly", to: &standardError)
		exit(1)
	}

	let fm = FileManager()
	let buildroot = CommandLine.Environment["DARWIN_BUILDROOT"] ?? CommandLine.workingDirectory
	if !(fm.directoryExists(atPath: joinPath(buildroot, "Roots")) &&
		fm.directoryExists(atPath: joinPath(buildroot, "Sources")) &&
		fm.directoryExists(atPath: joinPath(buildroot, "Symbols")) &&
		fm.directoryExists(atPath: joinPath(buildroot, "Headers")) &&
		fm.directoryExists(atPath: joinPath(buildroot, "Logs"))) {
		print("ERROR: Could not find darwinbuild root, this is required", to: &standardError)
		print("Please change your working directory to one initialized by:", to: &standardError)
		print("\t$ darwinbuild -init <plist>", to: &standardError)
		print("Alternatively, you may set the DARWIN_BUILDROOT environment variable to the", to: &standardError)
		print("absolute path of that directory.", to: &standardError)

		exit(1)
	}
	CommandLine.workingDirectory = buildroot

	var context = RecursiveBuildContext()
	if CommandLine.arguments[1] == "-g" {
		let groupOutput = splitLines(readProcessOutput(commandName: "darwinxref", arguments: ["group", CommandLine.arguments[2]]))
		for line in groupOutput {
			for word in line.components(separatedBy: NSCharacterSet.whitespaces) {
				if word != "" {
					context.buildProject(projectName: word)
				}
			}
		}
	} else if CommandLine.arguments[1] == "-p" {
		context.buildProject(projectName: CommandLine.arguments[2])
	} else {
		print("ERROR: First argument must be either -g or -p", to: &standardError)
		exit(1)
	}
}

main()
