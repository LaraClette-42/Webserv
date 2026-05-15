#!/usr/bin/perl
use strict;
use warnings;

my $method = $ENV{'REQUEST_METHOD'} || 'GET';
my $query  = $ENV{'QUERY_STRING'}   || '';

my $output;
if ($method eq 'POST') {
    my $length = int($ENV{'CONTENT_LENGTH'} || 0);
    my $body = '';
    read(STDIN, $body, $length) if $length > 0;
    $output = "<html><body><h1>POST received (Perl)</h1><p>$body</p></body></html>";
} elsif ($query) {
    $output = "<html><body><h1>GET with query (Perl)</h1><p>$query</p></body></html>";
} else {
    $output = "<html><body><h1>Hello from test.pl</h1><p>Perl CGI works.</p></body></html>";
}

print "Content-Type: text/html\r\n";
print "Content-Length: " . length($output) . "\r\n";
print "\r\n";
print $output;
