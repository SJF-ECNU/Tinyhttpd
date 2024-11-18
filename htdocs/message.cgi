#!/usr/bin/perl
use strict;
use warnings;
use CGI;

my $cgi = CGI->new;
print $cgi->header('text/plain; charset=utf-8');

my $message = $cgi->param('message') || '没有收到消息';
print "服务器收到消息: $message\n";
print "时间: " . localtime() . "\n"; 