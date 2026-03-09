#include "CGI.hpp"
#include "Utils.hpp"

CGI::CGI()
	: _pid(-1), _inputFd(-1), _outputFd(-1), _done(false),
	  _bodyWritten(false), _startTime(0), _bodyWriteOffset(0) {
}

CGI::~CGI() {
	closeFds();
	if (_pid > 0) {
		::kill(_pid, SIGKILL);
		waitpid(_pid, NULL, 0);
	}
}

void CGI::closeFds() {
	if (_inputFd >= 0) {
		close(_inputFd);
		_inputFd = -1;
	}
	if (_outputFd >= 0) {
		close(_outputFd);
		_outputFd = -1;
	}
}

int CGI::getOutputFd() const { return _outputFd; }
int CGI::getInputFd() const { return _inputFd; }
pid_t CGI::getPid() const { return _pid; }
bool CGI::isDone() const { return _done; }
time_t CGI::getStartTime() const { return _startTime; }
const std::string& CGI::getOutput() const { return _output; }

std::vector<std::string> CGI::_buildEnv(const Request& req,
										const LocationConfig& location,
										const ServerConfig& server,
										const std::string& scriptPath) {
	std::vector<std::string> env;

	env.push_back("REQUEST_METHOD=" + req.getMethod());
	env.push_back("QUERY_STRING=" + req.getQuery());
	env.push_back("CONTENT_TYPE=" + req.getHeader("Content-Type"));
	env.push_back("CONTENT_LENGTH=" + req.getHeader("Content-Length"));
	env.push_back("SCRIPT_NAME=" + req.getPath());
	env.push_back("SCRIPT_FILENAME=" + scriptPath);
	env.push_back("PATH_INFO=" + req.getPath());
	env.push_back("PATH_TRANSLATED=" + scriptPath);
	env.push_back("SERVER_NAME=" + server.host);
	env.push_back("SERVER_PORT=" + Utils::toString(server.port));
	env.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env.push_back("SERVER_SOFTWARE=" SERVER_NAME);
	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("REDIRECT_STATUS=200");

	// HTTP headers as environment variables
	const std::map<std::string, std::string>& headers = req.getHeaders();
	for (std::map<std::string, std::string>::const_iterator it = headers.begin();
		 it != headers.end(); ++it) {
		std::string envName = "HTTP_" + Utils::toUpper(it->first);
		for (size_t i = 0; i < envName.size(); ++i) {
			if (envName[i] == '-')
				envName[i] = '_';
		}
		env.push_back(envName + "=" + it->second);
	}

	(void)location;
	return env;
}

bool CGI::execute(const Request& req, const LocationConfig& location,
				  const ServerConfig& server, const std::string& scriptPath) {
	// Resolve to absolute path before forking
	std::string absScriptPath = scriptPath;
	if (!scriptPath.empty() && scriptPath[0] != '/') {
		char cwd[4096];
		if (getcwd(cwd, sizeof(cwd)))
			absScriptPath = std::string(cwd) + "/" + scriptPath;
	}

	int pipeIn[2];   // parent writes, child reads (stdin)
	int pipeOut[2];  // child writes, parent reads (stdout)

	if (pipe(pipeIn) < 0) {
		LOG_ERROR("CGI: pipe(pipeIn) failed");
		return false;
	}
	if (pipe(pipeOut) < 0) {
		LOG_ERROR("CGI: pipe(pipeOut) failed");
		close(pipeIn[0]);
		close(pipeIn[1]);
		return false;
	}

	_pid = fork();
	if (_pid < 0) {
		LOG_ERROR("CGI: fork() failed");
		close(pipeIn[0]);
		close(pipeIn[1]);
		close(pipeOut[0]);
		close(pipeOut[1]);
		return false;
	}

	if (_pid == 0) {
		// ─── Child process ───────────────────────────────────────────────
		close(pipeIn[1]);   // close write end of stdin pipe
		close(pipeOut[0]);  // close read end of stdout pipe

		dup2(pipeIn[0], STDIN_FILENO);
		dup2(pipeOut[1], STDOUT_FILENO);

		close(pipeIn[0]);
		close(pipeOut[1]);

		// Build environment
		std::vector<std::string> envVec = _buildEnv(req, location, server, absScriptPath);
		std::vector<char*> envp;
		for (size_t i = 0; i < envVec.size(); ++i)
			envp.push_back(const_cast<char*>(envVec[i].c_str()));
		envp.push_back(NULL);

		// chdir to script directory
		std::string scriptDir = absScriptPath;
		size_t lastSlash = scriptDir.rfind('/');
		if (lastSlash != std::string::npos) {
			scriptDir = scriptDir.substr(0, lastSlash);
			chdir(scriptDir.c_str());
		}

		// Execute CGI
		std::string cgiExec = location.cgiPath;
		char* args[3];
		args[0] = const_cast<char*>(cgiExec.c_str());
		args[1] = const_cast<char*>(absScriptPath.c_str());
		args[2] = NULL;

		execve(cgiExec.c_str(), args, &envp[0]);
		// If execve fails
		std::cerr << "CGI execve failed: " << cgiExec << std::endl;
		_exit(1);
	}

	// ─── Parent process ──────────────────────────────────────────────────
	close(pipeIn[0]);   // close read end of stdin pipe
	close(pipeOut[1]);  // close write end of stdout pipe

	_inputFd = pipeIn[1];   // we write the body here
	_outputFd = pipeOut[0]; // we read the response here

	// Set non-blocking
	fcntl(_inputFd, F_SETFL, O_NONBLOCK);
	fcntl(_outputFd, F_SETFL, O_NONBLOCK);

	_startTime = time(NULL);
	_done = false;
	_bodyWritten = false;

	return true;
}

void CGI::setBody(const std::string& body) {
	_bodyToWrite = body;
	_bodyWriteOffset = 0;
	if (body.empty()) {
		close(_inputFd);
		_inputFd = -1;
		_bodyWritten = true;
	}
}

bool CGI::writeBody() {
	if (_bodyWritten || _inputFd < 0)
		return true;

	size_t remaining = _bodyToWrite.size() - _bodyWriteOffset;
	if (remaining == 0) {
		close(_inputFd);
		_inputFd = -1;
		_bodyWritten = true;
		return true;
	}

	ssize_t n = write(_inputFd, _bodyToWrite.c_str() + _bodyWriteOffset, remaining);
	if (n > 0) {
		_bodyWriteOffset += n;
		if (_bodyWriteOffset >= _bodyToWrite.size()) {
			close(_inputFd);
			_inputFd = -1;
			_bodyWritten = true;
			return true;
		}
		return false;
	}
	if (n <= 0) {
		// Write failed or returned 0 -- close pipe, CGI gets truncated input
		close(_inputFd);
		_inputFd = -1;
		_bodyWritten = true;
		return true;
	}
	return false;
}

bool CGI::isBodyWritten() const {
	return _bodyWritten;
}

bool CGI::readOutput() {
	if (_outputFd < 0)
		return true;

	char buf[BUFFER_SIZE];
	ssize_t n = read(_outputFd, buf, sizeof(buf));

	if (n > 0) {
		_output.append(buf, n);
		return false;
	}

	if (n == 0) {
		// EOF -- CGI finished writing
		close(_outputFd);
		_outputFd = -1;
		// Reap child non-blocking
		reapChild();
		_done = true;
		return true;
	}

	// n < 0 -- EAGAIN or error
	return false;
}

bool CGI::reapChild() {
	if (_pid <= 0)
		return true;
	int status;
	pid_t ret = waitpid(_pid, &status, WNOHANG);
	if (ret > 0) {
		_pid = -1;
		return true;
	}
	return false;
}

bool CGI::checkTimeout() {
	if (_done)
		return false;
	return (time(NULL) - _startTime) >= CGI_TIMEOUT;
}

void CGI::kill() {
	if (_pid > 0) {
		::kill(_pid, SIGKILL);
		// Blocking waitpid after SIGKILL is safe -- child will die immediately
		int status;
		waitpid(_pid, &status, 0);
		_pid = -1;
	}
	closeFds();
	_done = true;
}
