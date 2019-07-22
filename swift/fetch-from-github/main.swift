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

class FetchSourcesCommand: Command {
	let name = "fetch-from-github"

	private let plistFilename = Param.Required<String>()
	private let projectName = Param.Required<String>()
	private let projectVersion = Param.Required<String>()
	private let sourcesDir = Param.Required<String>()

	func execute() throws {
		let data = try Data(contentsOf: URL(fileURLWithPath: plistFilename.value))
		let darwinbuildPlist = try PropertyListSerialization.propertyList(from: data, options: [], format: nil) as! [String : Any]

		let projects = darwinbuildPlist["projects"] as! [String: Any]
		if !projects.keys.contains(projectName.value) {
			print("ERROR: Project \(projectName.value) not found", to: &standardError)
			exit(1)
		}

		let projectData = projects[projectName.value] as! [String: Any]
		guard let githubData = projectData["github"] else {
			return
		}

		var downloadURL: String
		var repoBasename: String
		var tag: String

		if let githubRepoName = githubData as? String {
			let repoParts = githubRepoName.components(separatedBy: "/")
			repoBasename = repoParts[1]
			tag = projectVersion.value

			downloadURL = "https://github.com/\(githubRepoName)/archive/\(projectVersion.value).tar.gz"
		} else if let githubData = githubData as? [String: Any] {
			let githubRepoName = githubData["repo"] as! String
			let repoParts = githubRepoName.components(separatedBy: "/")
			repoBasename = repoParts[1]
			if let tagFromPlist = githubData["tag"] as? String {
				tag = tagFromPlist
				downloadURL = "https://github.com/\(githubRepoName)/archive/\(tag).tar.gz"
			} else {
				tag = projectVersion.value
				downloadURL = "https://github.com/\(githubRepoName)/archive/\(projectVersion.value).tar.gz"
			}
		} else {
			print("ERROR: github key must be of type string or dictionary", to: &standardError)
			exit(1)
		}

		let tarballFilename = projectName.value + "-" + projectVersion.value + ".tar.gz"
		let downloadPath = joinPath(sourcesDir.value, tarballFilename)
		print("Downloading \(downloadURL) ...")

		do {
			try Task.run("/usr/bin/curl", arguments: ["-fLs", "-o", downloadPath, downloadURL])
		} catch is RunError {
			// This can occur if the server returns a 404 or other error.
			exit(1)
		}

		if repoBasename != projectName.value || tag != projectVersion.value {
			let fm = FileManager()
			let tempDir = joinPath(fm.temporaryDirectory.path, "fetch-from-github")
			try fm.createDirectory(at: URL(fileURLWithPath: tempDir), withIntermediateDirectories: true, attributes: nil)

			do {
				try Task.run("/usr/bin/tar", arguments: ["xf", downloadPath], directory: tempDir)
				try fm.moveItem(at: URL(fileURLWithPath: joinPath(tempDir, repoBasename + "-" + tag)), to: URL(fileURLWithPath: joinPath(tempDir, projectName.value + "-" + projectVersion.value)))
				try Task.run("/usr/bin/tar", arguments: ["czf", tarballFilename, projectName.value + "-" + projectVersion.value], directory: tempDir)

				try? fm.removeItem(atPath: tempDir)
			} catch is RunError {
				try! fm.removeItem(atPath: tempDir)
				exit(1)
			}
		}
	}
}

func main() {
	let handler = CLI(singleCommand: FetchSourcesCommand())
	handler.goAndExit()
}

main()
