using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class merchant_apiDataManager
    {
        //merchant_apiViewModel
        //public int        id          { get; set; }
        //public string     name        { get; set; }
        //public string     url         { get; set; }
        //public string     login       { get; set; }
        //public string     password    { get; set; }
        //public string     api_json    { get; set; }
        //public string     options     { get; set; }
        //public int?       status      { get; set; }
        //public int        api_id      { get; set; }
        //public int?       b           { get; set; }

        public static void Add(merchant_apiViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new merchant_api
            {
                id      = model.id      ,  
                name    = model.name    ,
                url     = model.url     ,
                login   = model.login   ,
                password= model.password,
                api_json= model.api_json,
                options = model.options ,
                status  = model.status  ,
                api_id  = model.api_id  ,
                b       = model.b       ,
            };

            db.merchant_api.Add(dbmodel);
        }

        public static void Modify(merchant_apiViewModel model, RAD_PAYEntities db)
        {
            var result = db.merchant_api.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id               ;  
                    dbmodel.name = model.name           ;
                    dbmodel.url = model.url             ;
                    dbmodel.login = model.login         ;
                    dbmodel.password = model.password   ;
                    dbmodel.api_json = model.api_json   ;
                    dbmodel.options = model.options     ;
                    dbmodel.status = model.status       ;
                    dbmodel.api_id = model.api_id       ;
                    dbmodel.b = model.b;
                }
            }
        }

        public static void Delete(merchant_apiViewModel model, RAD_PAYEntities db)
        {
            var result = db.merchant_api.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.merchant_api.Remove(dbmodel);
                }
            }
        }

        public static List<merchant_apiViewModel> Get(merchant_apiViewModel model, RAD_PAYEntities db)
        {
            List<merchant_apiViewModel> list = null;

            var query = from resmodel in db.merchant_api
                        select new merchant_apiViewModel
                        {
                            id = resmodel.id,
                            name = resmodel.name,
                            url = resmodel.url,
                            login = resmodel.login,
                            password = resmodel.password,
                            api_json = resmodel.api_json,
                            options = resmodel.options,
                            status = resmodel.status,
                            api_id = resmodel.api_id,
                            b = resmodel.b,
                        };

            list = query.ToList();

            return list;
        }
    }
}