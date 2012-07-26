//
//  Olli's C++ Library
//  net/agent.cc
//  (c) 2012 Oliver Michel
//

#include <net/agent.h>

om::net::Agent::Agent() 
  : _interfaces(new std::map<int, om::net::IOInterface*>), 
    _timeout_mode(om::net::Agent::timeout_mode_none),
    _manual_timeout(0), _uniform_lower(0), _uniform_upper(0),
    _exponential_lambda(0), _current_timeout() {}

om::net::Agent::Agent(const om::net::Agent& copy_from) 
  : _interfaces(copy_from._interfaces), 
    _timeout_mode(copy_from._timeout_mode),
    _manual_timeout(copy_from._manual_timeout),
    _uniform_lower(copy_from._uniform_lower),
    _uniform_upper(copy_from._uniform_upper),
    _exponential_lambda(copy_from._exponential_lambda),
    _current_timeout(copy_from._current_timeout) {}

om::net::Agent& om::net::Agent::operator=(const om::net::Agent& copy_from) {

  _interfaces = copy_from._interfaces;
  _timeout_mode = copy_from._timeout_mode;
  _manual_timeout = copy_from._manual_timeout;
  _uniform_lower = copy_from._uniform_lower;
  _uniform_upper = copy_from._uniform_upper;
  _exponential_lambda = copy_from._exponential_lambda;
  _current_timeout = copy_from._current_timeout;

  return *this;
}

std::map<int, om::net::IOInterface*>* om::net::Agent::interfaces() const {

  return _interfaces;
}

void om::net::Agent::set_timeout_mode(om::net::Agent::timeout_mode_t t) {

  _timeout_mode = t;
}

om::net::Agent::timeout_mode_t om::net::Agent::timeout_mode() const {

  return _timeout_mode;
}

void om::net::Agent::set_manual_timeout(double t) {

  _manual_timeout = t;
}

double om::net::Agent::manual_timeout() {

  return _manual_timeout;
}

void om::net::Agent::set_uniform_lower(double l) {

  _uniform_lower = l;
}

double om::net::Agent::uniform_lower() {

  return _uniform_lower;
}

void om::net::Agent::set_uniform_upper(double u) {

  _uniform_upper = u;
}

double om::net::Agent::uniform_upper() {

  return _uniform_upper;
}

void om::net::Agent::set_exponential_lambda(double l) {

  _exponential_lambda = l;
}

double om::net::Agent::exponential_lambda() {

  return _exponential_lambda;
}

void om::net::Agent::add_interface(om::net::IOInterface* iface)
   throw(std::logic_error) {

  if(_interfaces->insert(std::make_pair(iface->fd(), iface)).second)
    iface->add_to_fd_set(&_fds), _fd_max = iface->fd();
  else
    throw std::logic_error("device is already added to this agent");
}

void om::net::Agent::clean_iface_fds() {

  // remove those fd's from _fds that do not belong to the configured sockets
  for(int fd = 0; fd <= _fd_max; fd++)
    if(_interfaces->find(fd) == _interfaces->end())
      FD_CLR(fd, &_fds);
}

void om::net::Agent::check_read_interfaces(timeval* timestamp) {

  // iterate over sockets and read data if available
  for(std::map<int,om::net::IOInterface*>::iterator i = _interfaces->begin();
    i != _interfaces->end(); ++i) {
    if(FD_ISSET(i->first, &_read_fds))
      this->device_ready(timestamp, (*_interfaces)[i->first]);
  }  
}

timeval om::net::Agent::next_timeout_timeval() 
  throw(std::invalid_argument, std::logic_error) {

  switch(_timeout_mode) {
    
    case om::net::Agent::timeout_mode_manual:
      if(fabs(_manual_timeout) <= 0.0001)
        throw std::invalid_argument("next_timeout_timeval(): manual t/o is 0");
      return om::tools::time::timeval_from_sec(_manual_timeout);
      break;
      
    case om::net::Agent::timeout_mode_uniform:

      if(fabs(_uniform_upper) <= 0.0001 && fabs(_uniform_upper) <= 0.0001)
        throw std::invalid_argument("next_timeout_timeval(): uniform args are 0");
      return om::tools::time::timeval_from_sec(
        om::tools::random::uniform_sample(_uniform_lower, _uniform_upper)
      );
      break;
    
    case om::net::Agent::timeout_mode_exponential:

      if(fabs(_exponential_lambda) <= 0.0001)
        throw std::invalid_argument("next_timeout_timeval(): exp args are 0");
      return om::tools::time::timeval_from_sec(
        om::tools::random::exponential_sample(_exponential_lambda)
      );      
      break;

    case om::net::Agent::timeout_mode_none:
      throw std::logic_error("next_timeout_timeval(): t/o mode set to none");
      break;
  }
}

void om::net::Agent::run()
  throw(std::runtime_error) {

  timeval* timeout_ptr = 0;
  timeval timestamp = {0,0}, timeout_copy = {0,0};
  int n_read_sockets = -1;

  this->clean_iface_fds();
  gettimeofday(&timestamp, 0);
  this->agent_start(&timestamp);

  while(1) { // main application loop

    _read_fds = _fds;

    if(_timeout_mode != om::net::Agent::timeout_mode_none) {
      _current_timeout = this->next_timeout_timeval();
      timeout_copy = _current_timeout, timeout_ptr = &_current_timeout;
    }

    // call select, pass null-pointers for write and error fds
    n_read_sockets = select(_fd_max+1, &_read_fds, 0, 0, timeout_ptr);

    gettimeofday(&timestamp, 0);

    if(n_read_sockets == 0) { // timeout triggered
      this->timeout_trigger(&timestamp, &timeout_copy);
    } else if(n_read_sockets > 0) { // n sockets ready for reading
      this->check_read_interfaces(&timestamp);
    } else if(n_read_sockets == -1) { // error occured
      throw std::runtime_error("select(): " + std::string(strerror(errno)));
    }
  }
}

om::net::Agent::~Agent() {

  delete _interfaces;
}
