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

fileprivate enum Exception: Error {
	case message(_ text: String)
}

internal extension Dictionary {
	func get<Type>(_ key: Key) -> Type? {
		if let value = self[key] {
			return value as? Type
		} else {
			return nil
		}
	}
}

class CodesignCommand: Command {
	let name = "darwinbuild-codesign"

	private let dstroot = SwiftCLI.Parameter()

	private enum TimestampType
	{
		case apple
		case custom(arg: String)
		case disabled
	}

	private func parseTimestampType(_ plistValue: Any) throws -> TimestampType {
		if let arg = plistValue as? String {
			return .custom(arg: arg)
		} else if let arg = plistValue as? Bool {
			if arg {
				return .apple
			} else {
				return .disabled
			}
		} else {
			throw Exception.message("Unrecognized timestamp value: must be either string or boolean")
		}
	}

	public func execute() throws {
		let dstroot = self.dstroot.value
		let codesignPlistPath = joinPath(dstroot, "usr", "local", "darwinbuild", "darwinbuild-codesign.plist")
		let data = try Data(contentsOf: URL(fileURLWithPath: codesignPlistPath))
		let codesignPlist = try PropertyListSerialization.propertyList(from: data, options: [], format: nil) as! [String: Any]

		guard let defaultCertificate: String = codesignPlist.get("certificate") else {
			print("ERROR: Default certificate must be provided (use \"certificate\" key in top-level of plist", to: &standardError)
			exit(1)
		}

		let defaultHardenedRuntime = codesignPlist.get("hardened_runtime") ?? false
		let defaultPrefix: String? = codesignPlist.get("prefix")
		let defaultTimestamp: TimestampType
		if let timestampValue: Any = codesignPlist.get("timestamp") {
			defaultTimestamp = try parseTimestampType(timestampValue)
		} else {
			defaultTimestamp = .apple
		}

		var signingMap: [Int: [Task]] = [:]
		guard let fileMap: [String: Any] = codesignPlist.get("files") else {
			print("Warning: Nothing to sign")
			return
		}

		for (key, data) in fileMap {
			let certificate: String
			let identifier: String?
			let prefix: String?
			let hardenedRuntime: Bool
			let dr: String?
			let order: Int
			let timestamp: TimestampType

			if let flag = data as? Bool {
				if !flag {
					print("Warning: false value interpreted as \"use all default values\"", to: &standardError)
				}

				certificate = defaultCertificate
				identifier = nil
				prefix = defaultPrefix
				hardenedRuntime = defaultHardenedRuntime
				dr = nil
				order = 0xFFFF
				timestamp = defaultTimestamp
			} else if let data = data as? [String: Any] {
				certificate = data.get("certificate") ?? defaultCertificate
				identifier = data.get("identifier")
				prefix = data.get("prefix") ?? defaultPrefix
				hardenedRuntime = (data["hardened_runtime"] as? Bool) ?? defaultHardenedRuntime
				dr = data.get("dr")
				order = data.get("order") ?? 0xFFFF
				if let timestampValue: Any = data.get("timestamp") {
					timestamp = try parseTimestampType(timestampValue)
				} else {
					timestamp = defaultTimestamp
				}
			} else {
				throw Exception.message("Values in \"files\" dictionary must be booleans or dictionaries only")
			}

			var codesignArgv = ["-s", certificate, "-f", "--preserve-metadata=entitlements"]
			if let identifier = identifier {
				codesignArgv.append("-i")
				codesignArgv.append(identifier)
			}
			if let prefix = prefix {
				codesignArgv.append("--prefix")

				if prefix.hasSuffix(".")  {
					codesignArgv.append(prefix)
				} else {
					codesignArgv.append(prefix + ".")
				}
			}
			if let dr = dr {
				codesignArgv.append("-r")
				codesignArgv.append(dr)
			}
			if hardenedRuntime {
				codesignArgv.append("-o")
				codesignArgv.append("runtime")
			}
			switch timestamp {
			case .apple:
				codesignArgv.append("--timestamp")
			case .disabled:
				codesignArgv.append("--timestamp=none")
			case .custom(let arg):
				codesignArgv.append("--timestamp=\(arg)")
			}
			codesignArgv.append(joinPath(dstroot, key))

			let task = Task(executable: "/usr/bin/codesign", arguments: codesignArgv, stdout: WriteStream.stdout, stderr: WriteStream.stderr)
			if var orderDict = signingMap[order] {
				orderDict.append(task)
				signingMap[order] = orderDict
			} else {
				signingMap[order] = [task]
			}
		}

		let keyVector = signingMap.keys.sorted()
		for index in keyVector {
			if let tasks = signingMap[index] {
				for task in tasks {
					let exitCode = task.runSync()
					if exitCode != 0 {
						print("Signing command failed with code \(exitCode)", to: &standardError)
						exit(1)
					}
				}
			}
		}
	}
}

func main() {
	let parser = CLI(singleCommand: CodesignCommand())
	parser.goAndExit()
}

main()
