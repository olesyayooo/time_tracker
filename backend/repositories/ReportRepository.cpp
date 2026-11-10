#include "ReportRepository.h"
#include <drogon/drogon.h>
#include <memory>
#include <sstream>

namespace time_tracker {
Json::Value ReportRepository::report(const std::string& owner,const EntryFilter& f,const std::string& group) const {
    // Один SQL-запрос: единый snapshot данных и единый as_of для всех интервалов.
    std::string buckets,groupColumns,item;
    if (group=="day") {
        buckets=R"(SELECT (day AT TIME ZONE 'Europe/Moscow')::date AS date,
            extract(epoch FROM least(finish,(((day AT TIME ZONE 'Europe/Moscow')::date+1)::timestamp AT TIME ZONE 'Europe/Moscow'))-greatest(beginning,day)) AS seconds
            FROM clipped CROSS JOIN LATERAL generate_series(
                date_trunc('day',beginning,'Europe/Moscow'),finish,interval '1 day','Europe/Moscow') AS day
            WHERE day<finish)";
        groupColumns="date";
        item="jsonb_build_object('date',date::text,'duration_seconds',round(sum(seconds))::bigint)";
    } else if (group=="task") {
        buckets="SELECT task_id,task_title,project_id,extract(epoch FROM finish-beginning) AS seconds FROM clipped";
        groupColumns="task_id,task_title,project_id";
        item="jsonb_build_object('task_id',task_id,'task_title',task_title,'project_id',project_id,'duration_seconds',round(sum(seconds))::bigint)";
    } else {
        buckets="SELECT project_id,project_name,extract(epoch FROM finish-beginning) AS seconds FROM clipped";
        groupColumns="project_id,project_name";
        item="jsonb_build_object('project_id',project_id,'project_name',project_name,'duration_seconds',round(sum(seconds))::bigint)";
    }
    const auto query=R"(
        WITH cfg AS (
            SELECT $2::date AS date_from,$3::date AS date_to,
                $2::date::timestamp AT TIME ZONE 'Europe/Moscow' AS lower_bound,
                ($3::date+1)::timestamp AT TIME ZONE 'Europe/Moscow' AS upper_bound,
                statement_timestamp() AS as_of
        ), clipped AS (
            SELECT p.id AS project_id,p.name AS project_name,t.id AS task_id,t.title AS task_title,
                greatest(e.started_at,c.lower_bound) AS beginning,
                least(coalesce(e.ended_at,c.as_of),c.upper_bound,c.as_of) AS finish
            FROM time_entries e JOIN tasks t ON t.id=e.task_id JOIN projects p ON p.id=t.project_id CROSS JOIN cfg c
            WHERE e.user_id=$1::uuid AND p.user_id=$1::uuid
                AND ($4='' OR p.id=NULLIF($4,'')::uuid) AND ($5='' OR t.id=NULLIF($5,'')::uuid)
                AND e.started_at<least(c.upper_bound,c.as_of)
                AND coalesce(e.ended_at,c.as_of)>c.lower_bound
        ), buckets AS ()" + buckets + "), grouped AS (SELECT " + item + " AS item,sum(seconds) AS seconds FROM buckets GROUP BY " + groupColumns + R"()
        SELECT jsonb_build_object(
            'from',c.date_from::text,'to',c.date_to::text,'timezone','Europe/Moscow',
            'as_of',to_char(c.as_of AT TIME ZONE 'UTC','YYYY-MM-DD"T"HH24:MI:SS.US"Z"'),
            'items',coalesce((SELECT jsonb_agg(item ORDER BY item::text) FROM grouped),'[]'::jsonb),
            'total_seconds',coalesce((SELECT round(sum(seconds))::bigint FROM grouped),0)
        )::text AS report FROM cfg c)";
    auto rows=drogon::app().getDbClient()->execSqlSync(query,owner,f.from,f.to,f.project_id,f.task_id);
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    const auto text=rows[0]["report"].as<std::string>();
    Json::Value result;
    std::string errors;
    if (!reader->parse(text.data(),text.data()+text.size(),&result,&errors)) throw std::runtime_error("Invalid report JSON");
    result["group_by"]=group;
    return result;
}
}
