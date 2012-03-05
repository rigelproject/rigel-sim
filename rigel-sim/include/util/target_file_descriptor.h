#ifndef __TARGET_FILE_DESCRIPTOR_H__
#define __TARGET_FILE_DESCRIPTOR_H__

#include <fcntl.h>
#include <sys/stat.h>
#include <cstdio>
#include <string>
#include <cassert>
#include <unistd.h>
#include <sstream>
#include <cerrno>
#include <map>
#include "rigelsim.pb.h"
#include "rigel_fcntl.h"
#include "rigel_sys_stat.h"

class TargetFileDescriptor;
typedef std::map<int, TargetFileDescriptor *> TargetFDMap; //Use pointers to avoid insertion overhead.

class TargetFileDescriptor {
  public:
    TargetFileDescriptor(rigelsim::TargetFileDescriptorState *tfds, int _hostfd = -1) : state(tfds), hostfd(_hostfd) { }
    TargetFileDescriptor(rigelsim::TargetFileDescriptorState *tfds, const std::string &path, int target_flag, mode_t target_mode);
    int open();
    int open_to_previous_offset();
    void save();
    int get_hostfd() const { return hostfd; }
    int has_path() const { return (state->has_path() && !state->path().empty()); }
    rigelsim::TargetFileDescriptorState *get_state() const { return state; }
  protected:
    void init_from_target(const std::string &path, int target_flag, mode_t target_mode);
    rigelsim::TargetFileDescriptorState *state;
    int hostfd;
};

TargetFileDescriptor::TargetFileDescriptor(rigelsim::TargetFileDescriptorState *tfds,
                                           const std::string &path,
                                           int target_flag,
                                           mode_t target_mode) : state(tfds)
{
  init_from_target(path, target_flag, target_mode);
}

void TargetFileDescriptor::init_from_target(const std::string &path, int target_flag, mode_t target_mode) {
  state->set_path(path);
  switch(target_flag & RS_O_ACCMODE) {
    case RS_O_RDONLY: state->set_access_mode(rigelsim::TargetFileDescriptorState::TFDS_O_RDONLY); break;
    case RS_O_WRONLY: state->set_access_mode(rigelsim::TargetFileDescriptorState::TFDS_O_WRONLY); break;
    case RS_O_RDWR: state->set_access_mode(rigelsim::TargetFileDescriptorState::TFDS_O_RDWR); break;
    default: assert(0 && "Unknown target-specified file access mode");
  }

  state->set_o_append(target_flag & RS_O_APPEND);
  state->set_o_creat(target_flag & RS_O_CREAT);
  state->set_o_excl(target_flag & RS_O_EXCL);
  state->set_o_noctty(target_flag & RS_O_NOCTTY);
  state->set_o_nonblock(target_flag & RS_O_NONBLOCK);
  state->set_o_sync(target_flag & RS_O_SYNC);
//FIXME make this check more intelligent
#ifdef __linux__
  state->set_o_rsync(target_flag & RS_O_RSYNC);
  state->set_o_dsync(target_flag & RS_O_DSYNC);
#else
  state->set_o_rsync(false);
  state->set_o_dsync(false);
#endif
  state->set_o_trunc(target_flag & RS_O_TRUNC);
  state->set_s_irusr(target_mode & RS_S_IRUSR);
  state->set_s_iwusr(target_mode & RS_S_IWUSR);
  state->set_s_ixusr(target_mode & RS_S_IXUSR);
  state->set_s_irgrp(target_mode & RS_S_IRGRP);
  state->set_s_iwgrp(target_mode & RS_S_IWGRP);
  state->set_s_ixgrp(target_mode & RS_S_IXGRP);
  state->set_s_iroth(target_mode & RS_S_IROTH);
  state->set_s_iwoth(target_mode & RS_S_IWOTH);
  state->set_s_ixoth(target_mode & RS_S_IXOTH);
  state->set_s_isuid(target_mode & RS_S_ISUID);
  state->set_s_isgid(target_mode & RS_S_ISGID);
  state->set_s_isvtx(target_mode & RS_S_ISVTX);
}

void TargetFileDescriptor::save()
{
  struct stat st;
  fstat(hostfd, &st);
  mode_t &m = st.st_mode;
  if(S_ISBLK(m))       state->set_file_type(rigelsim::TargetFileDescriptorState::TFDS_S_IFBLK);
  else if(S_ISCHR(m))  state->set_file_type(rigelsim::TargetFileDescriptorState::TFDS_S_IFCHR);
  else if(S_ISDIR(m))  state->set_file_type(rigelsim::TargetFileDescriptorState::TFDS_S_IFDIR);
  else if(S_ISFIFO(m)) state->set_file_type(rigelsim::TargetFileDescriptorState::TFDS_S_IFIFO);
  else if(S_ISREG(m))  state->set_file_type(rigelsim::TargetFileDescriptorState::TFDS_S_IFREG);
  else if(S_ISLNK(m))  state->set_file_type(rigelsim::TargetFileDescriptorState::TFDS_S_IFLNK);
  else if(S_ISSOCK(m)) state->set_file_type(rigelsim::TargetFileDescriptorState::TFDS_S_IFSOCK);
  else assert(0 && "Unknown file type!");

  state->set_s_irusr(m & S_IRUSR);
  state->set_s_iwusr(m & S_IWUSR);
  state->set_s_ixusr(m & S_IXUSR);
  state->set_s_irgrp(m & S_IRGRP);
  state->set_s_iwgrp(m & S_IWGRP);
  state->set_s_ixgrp(m & S_IXGRP);
  state->set_s_iroth(m & S_IROTH);
  state->set_s_iwoth(m & S_IWOTH);
  state->set_s_ixoth(m & S_IXOTH);
  state->set_s_isuid(m & S_ISUID);
  state->set_s_isgid(m & S_ISGID);
  state->set_s_isvtx(m & S_ISVTX);

  off_t off = lseek(hostfd, 0, SEEK_CUR);
  if(off == -1) {
    if(errno == ESPIPE) //Non-seekable (e.g., pipe, terminal)
      state->set_offset(0);
    else {
      std::stringstream s;
      s << "Unexpected error while getting offset into host fd " << hostfd;
      perror(s.str().c_str());
      assert(0 && "Error calling lseek()");
    }
  }
  else
    state->set_offset(off);
}

int TargetFileDescriptor::open() {
  int flag = 0;
  switch(state->access_mode()) {
    case rigelsim::TargetFileDescriptorState::TFDS_O_RDONLY: flag |= O_RDONLY; break;
    case rigelsim::TargetFileDescriptorState::TFDS_O_WRONLY: flag |= O_WRONLY; break;
    case rigelsim::TargetFileDescriptorState::TFDS_O_RDWR: flag |= O_RDWR; break;
    default: assert(0 && "Invalid access_mode value in TargetFileDescriptorState");
  }
  if(state->o_append()) flag |= O_APPEND;
  if(state->o_creat()) flag |= O_CREAT;
  if(state->o_excl()) flag |= O_EXCL;
  if(state->o_noctty()) flag |= O_NOCTTY;
  if(state->o_nonblock()) flag |= O_NONBLOCK;
  if(state->o_sync()) flag |= O_SYNC;
  if(state->o_trunc()) flag |= O_TRUNC;
//FIXME make this check more intelligent
#ifdef __linux__
  if(state->o_dsync()) flag |= O_DSYNC;
  if(state->o_rsync()) flag |= O_RSYNC;
#endif

  mode_t mode = 0;
  if(state->s_irusr()) mode |= S_IRUSR;
  if(state->s_iwusr()) mode |= S_IWUSR;
  if(state->s_ixusr()) mode |= S_IXUSR;
  if(state->s_irgrp()) mode |= S_IRGRP;
  if(state->s_iwgrp()) mode |= S_IWGRP;
  if(state->s_ixgrp()) mode |= S_IXGRP;
  if(state->s_iroth()) mode |= S_IROTH;
  if(state->s_iwoth()) mode |= S_IWOTH;
  if(state->s_ixoth()) mode |= S_IXOTH;
  if(state->s_isuid()) mode |= S_ISUID;
  if(state->s_isgid()) mode |= S_ISGID;
  if(state->s_isvtx()) mode |= S_ISVTX;

  int ret = ::open(state->path().c_str(), flag, mode);
  if(ret == -1) {
    std::stringstream s;
    s << "Error while opening file at '" << state->path() << "'";
    perror(s.str().c_str());
    assert(0);
  }
  hostfd = ret;
  return ret;
}

int TargetFileDescriptor::open_to_previous_offset() {
  int fd = open();
  if(fd == -1)
    return -1;
  off_t off = lseek(fd, (off_t)state->offset(), SEEK_SET);
  if(off == -1) {
    std::stringstream s;
    s << "Error while seeking file at '" << state->path() << "'"
      << " to byte " << state->offset();
    perror(s.str().c_str());
    assert(0);
  }
  hostfd = fd; //FIXME Redundant.  Remove?
  return fd;
}

#endif //#ifndef __TARGET_FILE_DESCRIPTOR_H__
