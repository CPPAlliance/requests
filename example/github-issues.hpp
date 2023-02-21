//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REQUESTS_EXAMPLE_GITHUB_ISSUES_HPP
#define BOOST_REQUESTS_EXAMPLE_GITHUB_ISSUES_HPP

#include <boost/describe/enum.hpp>
#include <boost/requests/json.hpp>
#include <boost/variant2.hpp>
#include <boost/container/pmr/unsynchronized_pool_resource.hpp>
#include <boost/url/format.hpp>

namespace github
{

using boost::variant2::monostate;
using boost::variant2::variant;
using boost::optional;
using boost::system::error_code;
using boost::system::result;

namespace urls = boost::urls;

// https://docs.github.com/en/rest/issues/issues
using string_view = boost::core::string_view;


#define GET_MEMBER(Name, Type)                                               \
  if (auto p = obj.if_contains(#Name))                                       \
  {                                                                          \
    if (auto v = boost::json::try_value_to<Type>(*p))                        \
      res.Name = *v;                                                         \
    else                                                                     \
      return v.error();                                                      \
  }                                                                          \
  else                                                                       \
  {                                                                          \
    static constexpr auto loc((BOOST_CURRENT_LOCATION));                     \
    return ::boost::system::error_code(boost::json::error::not_found, &loc); \
  }

#define GET_OPTIONAL_MEMBER(Name, Type)                                      \
  if (auto p = obj.if_contains(#Name))                                       \
  {                                                                          \
    if (!p->is_null())                                                       \
    {                                                                        \
      if (auto v = boost::json::try_value_to<Type>(*p))                      \
        res.Name = *v;                                                       \
      else                                                                   \
        return v.error();                                                    \
    }                                                                        \
  }


struct user_t
{
  std::string avatar_url;
  optional<std::string> email;  // optional
  std::string events_url;
  std::string followers_url;
  std::string following_url;
  std::string gists_url;
  optional<std::string> gravatar_id;
  std::string html_url;
  int id;
  std::string login;
  optional<std::string> name; // optional
  std::string node_id;
  std::string organizations_url;
  std::string received_events_url;
  std::string repos_url;
  bool site_admin;
  optional<std::string> starred_at;  // optional
  std::string starred_url;
  std::string subscriptions_url;
  std::string type;
  std::string url;
};

inline result<user_t> tag_invoke(boost::json::try_value_to_tag<user_t>, const boost::json::value & val)
{
  if (!val.is_object())
  {
    static constexpr auto loc((BOOST_CURRENT_LOCATION));
    return ::boost::system::error_code(boost::json::error::not_object, &loc);
  }

  auto & obj = val.get_object();

  user_t res;

  GET_MEMBER(avatar_url, std::string);
  GET_OPTIONAL_MEMBER(email, std::string);
  GET_MEMBER(events_url, std::string);
  GET_MEMBER(followers_url, std::string);
  GET_MEMBER(following_url, std::string);
  GET_MEMBER(gists_url, std::string);

  GET_OPTIONAL_MEMBER(gravatar_id, std::string);
  GET_MEMBER(html_url, std::string);

  GET_MEMBER(id, int);
  GET_OPTIONAL_MEMBER(name, std::string);
  GET_MEMBER(node_id, std::string);

  GET_MEMBER(organizations_url, std::string);
  GET_MEMBER(received_events_url, std::string);
  GET_MEMBER(repos_url, std::string);
  GET_MEMBER(site_admin, bool);
  GET_OPTIONAL_MEMBER(starred_at, std::string);
  GET_MEMBER(starred_url, std::string);
  GET_MEMBER(subscriptions_url, std::string);
  GET_MEMBER(type, std::string);
  GET_MEMBER(url, std::string);

  return res;
}

struct label // all members are optional
{
  optional<std::string> color;
  bool default_{false};
  optional<std::string> description;
  optional<int> id;
  optional<std::string>  name;
  optional<std::string>  node_id;
  optional<std::string>  url;
};


inline result<label> tag_invoke(boost::json::try_value_to_tag<label>, const boost::json::value & val)
{
  if (!val.is_object())
  {
    static constexpr auto loc((BOOST_CURRENT_LOCATION));
    return ::boost::system::error_code(boost::json::error::not_object, &loc);
  }

  auto & obj = val.get_object();

  label res;

  GET_OPTIONAL_MEMBER(color, std::string);

  if (auto p = obj.if_contains("default"))
  {
    if (auto v = p->if_bool())
      res.default_ = *v;
    else
    {
      static constexpr auto loc((BOOST_CURRENT_LOCATION));
      return ::boost::system::error_code(boost::json::error::not_found, &loc);
    }
  }

  GET_OPTIONAL_MEMBER(description, std::string);
  GET_OPTIONAL_MEMBER(id, int);
  GET_OPTIONAL_MEMBER(name, std::string);
  GET_OPTIONAL_MEMBER(node_id, std::string);
  return res;
}

BOOST_DEFINE_ENUM(author_association,
  COLLABORATOR,
  CONTRIBUTOR,
  FIRST_TIMER,
  FIRST_TIME_CONTRIBUTOR,
  MANNEQUIN,
  MEMBER,
  NONE,
  OWNER);



BOOST_DEFINE_ENUM(state_reason_t, completed, not_planned, reopened);
BOOST_DEFINE_ENUM(issue_state, open, closed);

struct milestone
{
  std::string url;
  std::string html_url;
  std::string labels_url;

  int id;
  std::string node_id;
  int number;
  issue_state state;
  std::string title;
  boost::optional<std::string> description;
  user_t creator;
  int open_issues;
  int closed_issues;
  std::string created_at;
  std::string updated_at;
  boost::optional<std::string> closed_at;
  boost::optional<std::string> due_on;
};


inline result<milestone> tag_invoke(boost::json::try_value_to_tag<milestone>, const boost::json::value & val)
{
  if (!val.is_object())
  {
    static constexpr auto loc((BOOST_CURRENT_LOCATION));
    return ::boost::system::error_code(boost::json::error::not_object, &loc);
  }

  auto & obj = val.get_object();

  milestone res;

  GET_MEMBER(url, std::string);
  GET_MEMBER(html_url, std::string);
  GET_MEMBER(labels_url, std::string);

  GET_MEMBER(id, int);
  GET_MEMBER(node_id, std::string);
  GET_MEMBER(number, int);
  GET_MEMBER(state, issue_state);
  GET_MEMBER(title, std::string);
  GET_OPTIONAL_MEMBER(description, std::string);
  GET_MEMBER(creator, user_t);
  GET_MEMBER(open_issues,   int);
  GET_MEMBER(closed_issues, int);
  GET_OPTIONAL_MEMBER(closed_at, std::string);
  GET_OPTIONAL_MEMBER(due_on, std::string);

  return res;
}


struct issue
{
  optional<std::string> active_lock_reason;
  optional<user_t> assignee;
  optional<std::vector<user_t>> users;
  /**
     * How the author is associated with the repository.
   */
  enum author_association author_association ;
  /**
     * Contents of the issue
   */
  optional<std::string> body;
  optional<std::string> body_html;
  optional<std::string> body_text;
  optional<std::string> closed_at;
  optional<user_t> closed_by;
  int comments;
  std::string comments_url;
  std::string created_at;
  bool draft = false;
  std::string events_url;
  std::string html_url;
  int id;
  /**
     * Labels to associate with this issue; pass one or more label names to replace the set of
     * labels on this issue; send an empty array to clear all labels from the issue; note that
     * the labels are silently dropped for users without push access to the repository
   */
  std::vector<label> labels;
  std::string labels_url;
  bool locked;
  optional<struct milestone> milestone;
  std::string node_id;
  /**
     * Number uniquely identifying the issue within its repository
   */
  int number;

  // we skip the details for the following objects
  optional<boost::json::value> performed_via_github_app;
  optional<boost::json::value> pull_request;
  optional<boost::json::value> reactions;

  /**
     * A repository on GitHub.
   */

  optional<boost::json::value> repository;
  std::string repository_url;
  /**
     * State of the issue; either 'open' or 'closed'
   */
  issue_state state;
  /**
     * The reason for the current state
   */
  optional<state_reason_t> state_reason;
  optional<std::string> timeline_url;
  /**
     * Title of the issue
   */
  std::string title;
  std::string updated_at;
  /**
     * URL for the issue
   */
  std::string url;
  optional<user_t> user;
};


inline result<issue> tag_invoke(boost::json::try_value_to_tag<issue>, const boost::json::value & val)
{
  if (!val.is_object())
  {
    static constexpr auto loc((BOOST_CURRENT_LOCATION));
    return ::boost::system::error_code(boost::json::error::not_object, &loc);
  }

  auto & obj = val.get_object();

  issue res;

  GET_OPTIONAL_MEMBER(active_lock_reason, std::string);
  GET_OPTIONAL_MEMBER(assignee, user_t);
  GET_OPTIONAL_MEMBER(author_association, author_association);

  GET_OPTIONAL_MEMBER(body, std::string);
  GET_OPTIONAL_MEMBER(body_html, std::string);
  GET_OPTIONAL_MEMBER(body_text, std::string);
  GET_OPTIONAL_MEMBER(closed_at, std::string);

  GET_OPTIONAL_MEMBER(closed_by, user_t);

  GET_MEMBER(comments, int);
  GET_MEMBER(comments_url, std::string);
  GET_MEMBER(created_at, std::string);
  GET_OPTIONAL_MEMBER(draft, bool);
  GET_MEMBER(events_url, std::string);
  GET_MEMBER(html_url, std::string);
  GET_MEMBER(id, int);
  GET_MEMBER(labels, std::vector<label>);

  GET_MEMBER(labels_url, std::string);
  GET_MEMBER(locked, bool);
  GET_OPTIONAL_MEMBER(milestone, milestone);
  GET_MEMBER(node_id, std::string);

  GET_MEMBER(number, int);

  GET_OPTIONAL_MEMBER(performed_via_github_app, boost::json::value);
  GET_OPTIONAL_MEMBER(pull_request,             boost::json::value);
  GET_OPTIONAL_MEMBER(reactions,                boost::json::value);
  GET_OPTIONAL_MEMBER(repository,               boost::json::value);

  GET_MEMBER(repository_url, std::string);
  GET_MEMBER(state, issue_state);

  GET_OPTIONAL_MEMBER(state_reason, state_reason_t);
  GET_OPTIONAL_MEMBER(timeline_url, std::string);

  GET_MEMBER(title,      std::string);
  GET_MEMBER(updated_at, std::string);

  GET_MEMBER(url, std::string);

  GET_OPTIONAL_MEMBER(user, user_t);

  return res;
}


using boost::requests::json::response;
using boost::core::string_view;

struct create_issue_options
{
  optional<std::string> body;
  optional<std::string> assignee;
  variant<monostate, std::string, int> milestone;
  std::vector<std::string> labels;
  std::vector<std::string> assignees;
};


inline void tag_invoke(boost::json::value_from_tag, boost::json::value & res, const create_issue_options & i)
{
  auto & obj = res.emplace_object();
  if (i.body) obj.insert_or_assign("body", *i.body);
  if (i.assignee) obj.insert_or_assign("assignee", *i.assignee);

  if (i.milestone.index() > 0) obj.insert_or_assign("milestone", boost::json::value_from(i.milestone, res.storage()));
  obj.insert_or_assign("labels",    boost::json::value_from(i.labels, res.storage()));
  obj.insert_or_assign("assignees", boost::json::value_from(i.assignees, res.storage()));
}


struct update_issue_options
{
  optional<std::string> body;
  optional<std::string> assignee;
  issue_state state = issue_state::open;
  optional<state_reason_t> state_reason;
  variant<monostate, std::string, int> milestone;
  std::vector<std::string> labels;
  std::vector<std::string> assignees;
};

inline void tag_invoke(boost::json::value_from_tag, boost::json::value & res, const update_issue_options & i)
{
  auto & obj = res.emplace_object();
  if (i.body) obj.insert_or_assign("body", *i.body);
  if (i.assignee) obj.insert_or_assign("assignee", *i.assignee);

  obj.insert_or_assign("state", boost::json::value_from(i.state, res.storage()));

  if (i.state_reason)          obj.insert_or_assign("state_reason", *i.state_reason);
  if (i.milestone.index() > 0) obj.insert_or_assign("milestone", boost::json::value_from(i.milestone, res.storage()));
  obj.insert_or_assign("labels",    boost::json::value_from(i.labels, res.storage()));
  obj.insert_or_assign("assignees", boost::json::value_from(i.assignees, res.storage()));

}


enum class lock_reason
{
  off_topic,
  too_heated,
  resolved,
  spam
};

inline void tag_invoke(boost::json::value_from_tag, boost::json::value & res, lock_reason lr)
{
  switch (lr)
  {
    case lock_reason::off_topic:  res = "off-topic";  break;
    case lock_reason::too_heated: res = "too-heated"; break;
    case lock_reason::resolved:   res = "resolved";   break;
    case lock_reason::spam:       res = "spam";       break;
  }
}

enum class issue_filter { assigned, created, mentioned, subscribed, repos, all };
enum class sort_t         { created, updated, comments };
enum class direction_t    { asc, desc };
enum class query_state  { open, close, all };

struct list_issues_query
{
  issue_filter filter = issue_filter::assigned;
  query_state state = query_state::open;
  optional<string_view> label;
  optional<sort_t> sort;
  optional<direction_t> direction;
  optional<string_view> since;
  int per_page = 30;
  int page = 1;

  auto make_query(boost::core::string_view pt) const -> boost::urls::url_view
  {
    static boost::urls::url storage;
    storage.set_path(pt);
    auto params = storage.params();
    params.clear();
    switch (filter)
    {
    default: break;
    case issue_filter::created:    params.set("filter", "created");    break;
    case issue_filter::mentioned:  params.set("filter", "mentioned");  break;
    case issue_filter::subscribed: params.set("filter", "subscribed"); break;
    case issue_filter::repos:      params.set("filter", "repos");      break;
    case issue_filter::all:        params.set("filter", "all");        break;
    }

    if (state == query_state::close) params.set("state", "close");
    if (state == query_state::all)   params.set("state", "all");

    if (label) params.set("label", *label);
    if (sort)
      switch (*sort)
      {
      case sort_t::created:   params.set("sort", "created");  break;
      case sort_t::updated:   params.set("sort", "updated");  break;
      case sort_t::comments:  params.set("sort", "comments"); break;
      }

    if (direction)
      switch (*direction)
      {
      case direction_t::asc:  params.set("direction", "asc");  break;
      case direction_t::desc: params.set("direction", "desc");  break;
      }

    if (since) params.set("since", *since);

    if (per_page != 30) params.set("per_page", std::to_string(per_page));
    if (page != 1)      params.set("page",     std::to_string(page));

    return storage;
  }
};

struct issue_client
{
  issue_client(
      boost::asio::io_context & ctx,
      const std::string & auth_token,
      const std::string & host_name = "api.github.com") : conn_(ctx.get_executor(), sslctx_)
  {
    conn_.set_host(host_name);
    boost::asio::ip::tcp::resolver res{ctx};
    conn_.connect(boost::asio::ip::tcp::endpoint(*(res.resolve(host_name, "https").begin())));
    settings_.fields = boost::requests::headers({
        {boost::requests::http::field::content_type, "application/vnd.github+json"},
        boost::requests::bearer(auth_token)
    }, &memory_);
  }

  // List issues assigned to the authenticated user
  response<std::vector<issue>> list_issues(list_issues_query opt = {})
  {
    return boost::requests::json::get<std::vector<issue>>(conn_, opt.make_query("/issues"), settings_);
  }
  response<std::vector<issue>> list_issues(list_issues_query opt, error_code & ec)
  {
    return boost::requests::json::get<std::vector<issue>>(conn_, opt.make_query("/issues"), settings_, ec);
  }

  template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<std::vector<issue>>)) CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<std::vector<issue>>))
    async_list_issues(list_issues_query opt, CompletionToken && completion_token)
  {
    return boost::requests::json::async_get<std::vector<issue>>(
        conn_, opt.make_query("/issues"), settings_, std::forward<CompletionToken>(completion_token));
  }

  // List organization issues assigned to the authenticated user
  response<std::vector<issue>> list_issues(std::string owner, list_issues_query opt = {})
  {
    return boost::requests::json::get<std::vector<issue>>(
        conn_,
        opt.make_query(urls::format("/repos/{owner}/issues", owner).encoded_target()),
        settings_);
  }

  response<std::vector<issue>> list_issues(std::string owner, list_issues_query opt,error_code & ec)
  {
    return boost::requests::json::get<std::vector<issue>>(
        conn_,
        opt.make_query(urls::format("/repos/{owner}/issues", owner).encoded_target()),
        settings_, ec);
  }

  template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<std::vector<issue>>)) CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<std::vector<issue>>))
    async_list_issues(std::string owner, list_issues_query opt, CompletionToken && completion_token)
  {
    return boost::requests::json::async_get<std::vector<issue>>(
        conn_,
        opt.make_query(urls::format("/repos/{owner}/issues", owner).encoded_target()),
        settings_, std::forward<CompletionToken>(completion_token));
  }

  // List repository issues
  response<std::vector<issue>> list_issues(std::string owner, std::string repository, list_issues_query opt = {})
  {
    return boost::requests::json::get<std::vector<issue>>(
        conn_, opt.make_query(urls::format("/repos/{owner}/{repository}/issues", owner, repository).encoded_target()), settings_);
  }
  response<std::vector<issue>> list_issues(std::string owner, std::string repository, list_issues_query opt, error_code & ec)
  {
    return boost::requests::json::get<std::vector<issue>>(
        conn_, opt.make_query(urls::format("/repos/{owner}/{repository}/issues", owner, repository).encoded_target()),
        settings_, ec);

  }
  template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<std::vector<issue>>)) CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<std::vector<issue>>))
    async_list_issues(std::string owner, std::string repository, list_issues_query opt, CompletionToken && completion_token)
  {
    return boost::requests::json::async_get<std::vector<issue>>(
        conn_, opt.make_query(urls::format("/repos/{owner}/{repository}/issues", owner, repository).encoded_target()),
        settings_, std::forward<CompletionToken>(completion_token));

  }

  // Create an issue
  response<issue> create_issue(std::string owner, std::string repository, std::string title, create_issue_options opts = {})
  {
    return boost::requests::json::post<issue>(
        conn_,
        urls::format("/repos/{owner}/{repository}/issues", owner, repository),
        boost::json::value_from(opts, storage()), settings_);
  }
  response<issue> create_issue(std::string owner, std::string repository, std::string title, create_issue_options opts, error_code & ec)
  {
    return boost::requests::json::post<issue>(
        conn_,
        urls::format("/repos/{owner}/{repository}/issues", owner, repository),
        boost::json::value_from(opts, storage()), settings_, ec);
  }
  template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<issue>)) CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<issue>))
    async_create_issue(std::string owner, std::string repository, std::string title, create_issue_options opts,
                       CompletionToken && completion_token)
  {
    return boost::requests::json::async_post<issue>(
        conn_,
        urls::format("/repos/{owner}/{repository}/issues", owner, repository),
        boost::json::value_from(opts, storage()),
        settings_, std::forward<CompletionToken>(completion_token));
  }


  // Get an issue
  response<issue> get_issue(std::string owner, std::string repository, int issue_number)
  {
    return boost::requests::json::get<issue>(
        conn_,
        urls::format("/repos/{owner}/{repository}/issues/{issue_number}", owner, repository, issue_number),
        settings_);
  }
  response<issue> get_issue(std::string owner, std::string repository, int issue_number, error_code & ec)
  {
    return boost::requests::json::get<issue>(
        conn_,
        urls::format("/repos/{owner}/{repository}/issues/{issue_number}", owner, repository, issue_number),
        settings_, ec);
  }

  template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<issue>)) CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<issue>))
    async_get_issue(std::string owner, std::string repository, int issue_number,
                    CompletionToken && completion_token)
  {
    return boost::requests::json::async_get<issue>(
        conn_,
        urls::format("/repos/{owner}/{repository}/issues/{issue_number}", owner, repository, issue_number),
        settings_, std::forward<CompletionToken>(completion_token));
  }


  // Update an issue
  response<issue> update_issue(std::string owner, std::string repository, int issue_number, update_issue_options opts = {})
  {
    return boost::requests::json::post<issue>(
        conn_,
        urls::format("/repos/{owner}/{repository}/issues/{issue_number}", owner, repository, issue_number),
        boost::json::value_from(opts, storage()),
        settings_);
  }
  response<issue> update_issue(std::string owner, std::string repository, int issue_number, update_issue_options opts, error_code & ec)
  {
    return boost::requests::json::post<issue>(
        conn_,
        urls::format("/repos/{owner}/{repository}/issues/{issue_number}", owner, repository, issue_number),
        boost::json::value_from(opts, storage()),
        settings_, ec);
  }

  template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, response<issue>)) CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, response<issue>))
  async_update_issue(std::string owner, std::string repository, int issue_number, update_issue_options opts,
                     CompletionToken && completion_token)
  {
    return boost::requests::json::async_post<issue>(
        conn_,
        urls::format("/repos/{owner}/{repository}/issues/{issue_number}", owner, repository, issue_number),
        boost::json::value_from(opts, storage()),
        settings_, std::forward<CompletionToken>(completion_token));
  }

  // Lock an issue
  boost::requests::response lock_issue(std::string owner, std::string repository, int issue_number, lock_reason reason)
  {
    return boost::requests::put(
        conn_,
        urls::format("/repos/{owner}/{repository}/issues/{issue_number}/lock", owner, repository, issue_number),
        boost::json::value({{"lock_reason", boost::json::value_from(reason, storage())}}, storage()), settings_);
  }
  boost::requests::response lock_issue(std::string owner, std::string repository, int issue_number, lock_reason reason, error_code & ec)
  {
    return boost::requests::put(
        conn_,
        urls::format("/repos/{owner}/{repository}/issues/{issue_number}/lock", owner, repository, issue_number),
        boost::json::value({{"lock_reason", boost::json::value_from(reason, storage())}}, storage()), settings_, ec);
  }

  template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, boost::requests::response)) CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, boost::requests::response))
    async_lock_issue(std::string owner, std::string repository, int issue_number, lock_reason reason,
                     CompletionToken && completion_token)
  {
    return boost::requests::async_put(
        conn_,
        urls::format("/repos/{owner}/{repository}/issues/{issue_number}/lock", owner, repository, issue_number),
        boost::json::value{{{"lock_reason", boost::json::value_from(reason, storage())}}, storage()}, settings_,
        std::forward<CompletionToken>(completion_token));
  }
  // Unlock an issue
  boost::requests::response unlock_issue(std::string owner, std::string repository, int issue_number)
  {
    return boost::requests::delete_(
        conn_,
        urls::format("/repos/{owner}/{repository}/issues/{issue_number}/lock", owner, repository, issue_number),
        settings_);
  }

  boost::requests::response unlock_issue(std::string owner, std::string repository, int issue_number, error_code & ec)
  {
    return boost::requests::delete_(
        conn_,
        urls::format("/repos/{owner}/{repository}/issues/{issue_number}/lock", owner, repository, issue_number),
        settings_, ec);
  }

  template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, boost::requests::response)) CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, boost::requests::response))
  async_unlock_issue(std::string owner, std::string repository, int issue_number,
                     CompletionToken && completion_token)
  {
    return boost::requests::async_delete(
        conn_,
        urls::format("/repos/{owner}/{repository}/issues/{issue_number}/lock", owner, repository, issue_number), settings_,
        std::forward<CompletionToken>(completion_token));
  }

  // List user account issues assigned to the authenticated user
  response<std::vector<issue>> list_user_issues(list_issues_query opt = {})
  {
    return boost::requests::json::get<std::vector<issue>>(conn_, opt.make_query("/user/issues"), settings_);
  }
  response<std::vector<issue>> list_user_issues(list_issues_query opt, error_code & ec)
  {
    return boost::requests::json::get<std::vector<issue>>(conn_, opt.make_query("/user/issues"), settings_, ec);
  }


  template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code, boost::requests::response)) CompletionToken>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void (boost::system::error_code, boost::requests::response))
  async_list_user_issues(list_issues_query opt,
                         CompletionToken && completion_token)
  {
    return boost::requests::json::async_get<std::vector<issue>>(conn_, opt.make_query("/user/issues"), settings_,
                                                                std::forward<CompletionToken>(completion_token));
  }


  boost::json::storage_ptr storage()
  {
    return boost::json::storage_ptr(&memory_);
  }
private:
  boost::container::pmr::unsynchronized_pool_resource memory_;

  boost::requests::cookie_jar jar_;
  boost::requests::request_parameters settings_{boost::requests::http::fields {&memory_},
                                              {true, boost::requests::redirect_mode::none, 0}, &jar_};



  boost::asio::ssl::context sslctx_{boost::asio::ssl::context_base::tls_client};
  boost::requests::connection conn_;
};


}

#endif // BOOST_REQUESTS_EXAMPLE_GITHUB_ISSUES_HPP
