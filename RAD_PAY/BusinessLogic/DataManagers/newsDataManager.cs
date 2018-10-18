using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class newsDataManager
    {
        //newsViewModel
//publiclongid{get;set;}
//publicstringmsg{get;set;}
//publicDateTime?add_ts{get;set;}
//publicDateTime?edit_ts{get;set;}
//publicintlang{get;set;}
//publiclonguid{get;set;}

        public static void Add(newsViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new news
            {
                id      = model.id      ,
                msg     = model.msg     ,
                add_ts  = model.add_ts  ,
                edit_ts = model.edit_ts ,
                lang    = model.lang    ,
                uid     = model.uid     ,
            };

            db.news.Add(dbmodel);
        }

        public static void Modify(newsViewModel model, RAD_PAYEntities db)
        {
            var result = db.news.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id      ;
                    dbmodel.msg = model.msg     ;
                    dbmodel.add_ts = model.add_ts  ;
                    dbmodel.edit_ts = model.edit_ts ;
                    dbmodel.lang = model.lang    ;
                    dbmodel.uid = model.uid     ;
                }
            }
        }

        public static void Delete(newsViewModel model, RAD_PAYEntities db)
        {
            var result = db.news.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.news.Remove(dbmodel);
                }
            }
        }

        public static List<newsViewModel> Get(newsViewModel model, RAD_PAYEntities db)
        {
            List<newsViewModel> list = null;

            var query = from resmodel in db.news
                        select new newsViewModel
                        {
                            id = resmodel.id,
                            msg = resmodel.msg,
                            add_ts = resmodel.add_ts,
                            edit_ts = resmodel.edit_ts,
                            lang = resmodel.lang,
                            uid = resmodel.uid,
                        };

            list = query.ToList();

            return list;
        }
    }
}