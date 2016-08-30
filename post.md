---
title: News
---

{% for post in site.posts %}
<h2><a href="{{ post.url | prepend: relative_root_url }}">{{ post.title }}</a> <small>{{ post.date | date: "%B %-d, %Y" }}</small></h2>
{{ post.content }}
{% endfor %}
