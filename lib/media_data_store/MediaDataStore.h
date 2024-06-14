/** 
 *  File: ap_airplay_service.cpp
 *  Project: apsdk
 *  Created: Oct 25, 2018
 *  Author: Sheen Tian
 *  
 *  This file is part of apsdk (https://github.com/air-display/apsdk-public) 
 *  Copyright (C) 2018-2024 Sheen Tian 
 *  
 *  apsdk is free software: you can redistribute it and/or modify it under the terms 
 *  of the GNU General Public License as published by the Free Software Foundation, 
 *  either version 3 of the License, or (at your option) any later version.
 *  
 *  apsdk is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  See the GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License along with this. 
 *  If not, see <https://www.gnu.org/licenses/>.
 *==============================================================================
 * modified by fduncanh (2024)                                                                                                                                        * based on class ap_casting_media_data_store of                                                                                                                     
 * http://github.com/air-display/apsdk-public                                                                                                                         */                                               

#pragma once
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <vector>
#include <mutex>

class MediaDataStore {
  /// <summary>
  ///
  /// </summary>
  enum app_id {
    e_app_youtube = 0,
    e_app_netflix = 1,
    e_app_unknown = (uint32_t)-1,
  };

  /// <summary>
  ///
  /// </summary>
  static constexpr const char *MLHLS_SCHEME = "mlhls://";

  /// <summary>
  ///
  /// </summary>
  static constexpr const char *NFHLS_SCHEME = "nfhls://";

  /// <summary>
  ///
  /// </summary>
  static constexpr const char *NFLX_VIDEO = "nflxvideo";

  /// <summary>
  ///
  /// </summary>
  static constexpr const char *SCHEME_LIST = "mlhls://|nfhls://";

  /// <summary>
  ///
  /// </summary>
  static constexpr const char *HOST_LIST = "localhost|127\\.0\\.0\\.1";

  /// <summary>
  ///
  /// </summary>
  static constexpr const char *MLHLS_HOST = "mlhls://localhost";

  /// <summary>
  ///
  /// </summary>
  static constexpr const char *MASTER_M3U8 = "master.m3u8";

  /// <summary>
  ///
  /// </summary>
  static constexpr const char *INDEX_M3U8 = "index.m3u8";

  /// <summary>
  ///
  /// </summary>
  static constexpr const char *HTTP_SCHEME = "http://";

  /// <summary>
  ///
  /// </summary>
  typedef std::map<std::string, std::string> media_data;

private:
  
  app_id app_id_;
  uint32_t request_id_;
  std::string session_id_;
  std::string primary_uri_;
  std::stack<std::string> uri_stack_;

  std::string playback_uuid_;
  float start_pos_in_ms_;
  std::string host_;
  media_data media_data_;
  std::mutex mtx_;
  int socket_fd_;

public:

  MediaDataStore();

  ~MediaDataStore();

  static MediaDataStore &get();

  void set_store_root(uint16_t port, int socket_fd); 

  const char* get_session_id () {
    return session_id_.c_str();
  }
  
  void set_session_id(const char * session_id) {
    session_id_ = session_id;
  }

  const char* get_playback_uuid () {
    return playback_uuid_.c_str();
  }
  
  void set_playback_uuid(const char * playback_uuid) {
    playback_uuid_ = playback_uuid;
  }

  float get_start_pos_in_ms() {
    return start_pos_in_ms_;
  }
  
  void set_start_pos_in_ms(float start_pos_in_ms) {
    start_pos_in_ms_ = start_pos_in_ms;
  }
  
  // request media data from client side
  bool request_media_data(const std::string &primary_uri, const std::string &session_id);

  // generate and store the media data
  std::string process_media_data(const std::string &uri, const char *data, int datalen);

  // serve the media data for player
  std::string query_media_data(const std::string &path);

  void reset();

protected:
  static app_id get_appi_id(const std::string &uri);

  void add_media_data(const std::string &uri, const std::string &data);

  static bool is_primary_data_uri(const std::string &uri);

  void send_fcup_request(const char * uri, int request_id, const char * session_id_str, int socket_id);

  std::string adjust_primary_uri(const std::string &uri);

  std::string extract_uri_path(const std::string &uri);

  std::string adjust_primary_media_data(const std::string &data);

  static std::string adjust_secondary_media_data(const std::string &data);

  // For Youtube
  std::string adjust_mlhls_data(const std::string &data);

  // For Netflix
  std::string adjust_nfhls_data(const std::string &data);

};
